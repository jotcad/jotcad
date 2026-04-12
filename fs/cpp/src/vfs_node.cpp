#include "../include/vfs_node.h"
#include "../vendor/httplib.h"
#include "../vendor/sha256.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>

namespace jotcad {
namespace fs {

namespace fs = std::filesystem;

VFSNode::VFSNode(const Config& config) : config_(config) {
    if (!config_.storage_dir.empty()) {
        if (!fs::exists(config_.storage_dir)) {
            fs::create_directories(config_.storage_dir);
        } else {
            std::cout << "[VFSNode " << config_.id << "] EPHEMERAL WIPE: Cleaning storage directory: " << config_.storage_dir << std::endl;
            for (const auto& entry : fs::directory_iterator(config_.storage_dir)) {
                if (entry.path().extension() == ".data" || entry.path().extension() == ".meta") {
                    fs::remove(entry.path());
                }
            }
        }
    }
    server_ptr_ = new httplib::Server();
}

VFSNode::~VFSNode() {
    stop();
    delete static_cast<httplib::Server*>(server_ptr_);
}

namespace {
std::vector<uint8_t> safe_to_bytes(const std::string& s) {
    try {
        if (s.size() > (size_t)1024 * 1024 * 1024) { // 1GB limit for sanity
             throw std::length_error("String exceeds 1GB limit for VFS conversion");
        }
        return std::vector<uint8_t>(s.begin(), s.end());
    } catch (const std::exception& e) {
        std::cerr << "[VFSNode] CRITICAL ERROR in safe_to_bytes: " << e.what() << " (string size: " << s.size() << ")" << std::endl;
        throw;
    }
}
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    handlers_[path] = handler;
    if (schema.is_null() || schema.empty()) {
        std::cerr << "[VFSNode " << config_.id << "] ERROR: Missing or empty schema for: " << path << std::endl;
    } else {
        schemas_[path] = schema;
        // Proactive write for schema
        try {
            std::string serialized = schema.dump();
            std::vector<uint8_t> data = safe_to_bytes(serialized);
            write("sys/schema", {{"target", path}}, data);
        } catch (const std::exception& e) {
             std::cerr << "[VFSNode " << config_.id << "] ERROR: Schema serialization or write failed: " << e.what() << std::endl;
        }
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(reverse_mutex_);
    reverse_listeners_[peer_id] = &res;
}

void VFSNode::add_peer(const std::string& url) {
    if (url.empty()) return;
    
    // 1. Check idempotency
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (connecting_.count(url)) return;
        connecting_.insert(url);
    }

    try {
        std::cout << "[VFSNode " << config_.id << "] Handshaking with " << url << "..." << std::endl;
        
        httplib::Client cli(url);
        cli.set_connection_timeout(std::chrono::seconds(2));
        
        json body = {
            {"id", config_.id},
            {"url", "http://localhost:" + std::to_string(config_.port)}
        };

        if (auto res = cli.Post("/register", body.dump(), "application/json")) {
            if (res->status == 200) {
                auto info = json::parse(res->body);
                std::string remote_id = info.at("id");

                if (!remote_id.empty() && remote_id != config_.id) {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    peers_[remote_id] = url;
                    std::cout << "[VFSNode " << config_.id << "] Handshake success. Peer: " << remote_id << std::endl;
                    
                    if (info.at("reachability") == "REVERSE") {
                        // TODO: Implement client-side listen loop if C++ nodes need to reach back through NAT
                    }
                }
            }
        }
    } catch (...) {}

    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        connecting_.erase(url);
    }
}

void VFSNode::listen() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);

    svr->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"status", "OK"}, {"id", config_.id}}).dump(), "application/json");
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        VFSRequest vfs_req;
        try {
            auto body = json::parse(req.body);
            vfs_req.path = body.at("path").get<std::string>();
            vfs_req.parameters = body.value("parameters", json::object());
            vfs_req.stack = body.value("stack", std::vector<std::string>());
            vfs_req.expiresAt = body.value("expiresAt", 0LL);
            
            std::cout << "[VFSNode " << config_.id << "] Incoming READ: " << vfs_req.path << " params: " << vfs_req.parameters.dump() << std::endl;

            auto data = read(vfs_req);
            if (!data.empty()) {
                std::cout << "[VFSNode " << config_.id << "] READ SUCCESS: " << vfs_req.path << " (" << data.size() << " bytes)" << std::endl;
                res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
            } else {
                std::cout << "[VFSNode " << config_.id << "] READ FAILED (Not Found): " << vfs_req.path << std::endl;
                res.status = 404;
            }
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] HTTP READ ERROR in JSON parse or logic: " << e.what() << " (Request body was size " << req.body.size() << ")" << std::endl;
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        } catch (...) {
            std::cerr << "[VFSNode " << config_.id << "] HTTP READ ERROR: Unknown exception" << std::endl;
            res.status = 400;
        }
    });

    svr->Post("/spy", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            VFSRequest vfs_req;
            vfs_req.path = body.at("path").get<std::string>();
            vfs_req.parameters = body.value("parameters", json::object());
            
            auto data = spy(vfs_req);
            if (!data.empty()) {
                res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
                res.set_header("x-vfs-id", config_.id);
            } else {
                res.status = 404;
                res.set_content("Not Found", "text/plain");
            }
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/register", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string peer_id = body.at("id").get<std::string>();
            std::string peer_url = body.value("url", "");
            
            bool can_reach = false;
            if (!peer_url.empty()) {
                httplib::Client cli(peer_url);
                cli.set_connection_timeout(std::chrono::seconds(1));
                if (auto h_res = cli.Get("/health")) {
                    can_reach = h_res->status == 200;
                }
            }

            if (can_reach && !peer_url.empty()) {
                // Background handshake back to confirm linkage
                std::thread([this, peer_url]() { this->add_peer(peer_url); }).detach();
            }

            res.set_content(json({
                {"id", config_.id},
                {"reachability", can_reach ? "DIRECT" : "REVERSE"}
            }).dump(), "application/json");
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) {
            res.status = 400;
            res.set_content("Missing x-vfs-peer-id", "text/plain");
            return;
        }

        this->register_reverse_peer(peer_id, res);
    });

    // Start neighbor handshakes in background
    for (const auto& url : config_.neighbors) {
        std::thread([this, url]() { this->add_peer(url); }).detach();
    }

    std::cout << "[VFSNode " << config_.id << "] Listening on 0.0.0.0:" << config_.port << std::endl;
    svr->listen("0.0.0.0", config_.port);
}

std::vector<uint8_t> VFSNode::spy(const VFSRequest& req) {
    std::vector<uint8_t> result;
    std::cout << "[VFSNode " << config_.id << "] Spy request for: " << req.path << std::endl;
    
    // 1. Local passive scan
    if (fs::exists(config_.storage_dir)) {
        for (const auto& entry : fs::directory_iterator(config_.storage_dir)) {
            if (entry.path().extension() == ".meta") {
                try {
                    std::ifstream is(entry.path());
                    json meta = json::parse(is);
                    
                    // Simplified matching: check if meta matches req.path
                    if (meta["path"].get<std::string>().find(req.path) != std::string::npos) {
                        fs::path p = entry.path();
                        p.replace_extension(".data");
                        
                        auto data = get_local(p.stem().string());
                        if (data.empty()) continue;
                        
                        std::cout << "[VFSNode " << config_.id << "] Spy match found: " << meta["path"] << std::endl;
                        
                        // Format as VFS chunk
                        std::string header = "\n=" + std::to_string(data.size()) + " " + meta.dump() + "\n";
                        result.insert(result.end(), header.begin(), header.end());
                        result.insert(result.end(), data.begin(), data.end());
                    }
                } catch (...) {}
            }
        }
    }
    
    return result;
}

void VFSNode::stop() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);
    if (svr->is_running()) svr->stop();
}

bool VFSNode::validate_selector(const VFSRequest& req, std::string& error_out) {
    if (!schemas_.count(req.path)) return true; // Assume permissive

    const json& schema = schemas_[req.path];
    const json& params = req.parameters;

    if (schema.contains("required")) {
        for (const auto& req_field : schema["required"]) {
            std::string field_name = req_field.get<std::string>();
            if (!params.contains(field_name)) {
                error_out = "Missing required parameter: " + field_name;
                return false;
            }
        }
    }

    if (schema.contains("properties")) {
        for (auto it = params.begin(); it != params.end(); ++it) {
            std::string key = it.key();
            if (schema["properties"].contains(key)) {
                const json& prop_schema = schema["properties"][key];
                if (prop_schema.contains("type")) {
                    std::string type = prop_schema["type"];
                    if (type == "number" && !it.value().is_number()) {
                        error_out = "Invalid type for parameter '" + key + "': Expected number";
                        return false;
                    }
                    if (type == "string" && !it.value().is_string()) {
                        error_out = "Invalid type for parameter '" + key + "': Expected string";
                        return false;
                    }
                    if (type == "object" && !it.value().is_object()) {
                        error_out = "Invalid type for parameter '" + key + "': Expected object";
                        return false;
                    }
                }
                if (prop_schema.contains("enum")) {
                    bool found = false;
                    for (const auto& e : prop_schema["enum"]) {
                        if (e == it.value()) { found = true; break; }
                    }
                    if (!found) {
                        error_out = "Invalid value for parameter '" + key + "'";
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

std::vector<uint8_t> VFSNode::read(const VFSRequest& req) {
    // TTL Enforcement
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    if (req.expiresAt > 0 && now > req.expiresAt) {
        std::cerr << "[VFSNode " << config_.id << "] REJECTED Stale Request: " << req.path 
                  << " (Expired " << (now - req.expiresAt) << "ms ago)" << std::endl;
        return {};
    }

    std::string error;
    if (!validate_selector(req, error)) {
        std::cerr << "[VFSNode " << config_.id << "] REJECTED Underconstrained Request: " << error << std::endl;
        return {};
    }

    std::string cid = get_cid(req.path, req.parameters);

    if (has_local(cid)) return get_local(cid);

    if (handlers_.count(req.path)) {
        std::cout << "[VFSNode " << config_.id << "] Invoking handler for: " << req.path << std::endl;
        
        try {
            auto data = handlers_[req.path](req);
            
            if (!data.empty()) {
                std::cout << "[VFSNode " << config_.id << "] Handler SUCCESS: " << req.path << " (" << data.size() << " bytes)" << std::endl;
                write_local(cid, data, req.path, req.parameters);
                return data;
            } else {
                std::cout << "[VFSNode " << config_.id << "] Handler returned EMPTY data: " << req.path << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] Handler EXCEPTION: " << req.path << " - " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[VFSNode " << config_.id << "] Handler UNKNOWN EXCEPTION: " << req.path << std::endl;
        }
    }

    std::vector<std::string> new_stack = req.stack;
    new_stack.push_back(config_.id);

    for (const auto& neighbor : config_.neighbors) {
        try {
            httplib::Client cli(neighbor);
            cli.set_read_timeout(30, 0);
            
            json req_body = {
                {"path", req.path},
                {"parameters", req.parameters},
                {"stack", new_stack},
                {"expiresAt", req.expiresAt}
            };

            auto res = cli.Post("/read", req_body.dump(), "application/json");
            if (res && res->status == 200) {
                try {
                    std::vector<uint8_t> data = safe_to_bytes(res->body);
                    
                    // Size Verification
                    std::string len_str = res->get_header_value("Content-Length");
                    if (!len_str.empty()) {
                        size_t expected = std::stoull(len_str);
                        if (data.size() != expected) {
                            std::cerr << "[VFSNode " << config_.id << "] REJECTED Aborted Mesh Stream: " 
                                      << req.path << " Expected " << expected << " got " << data.size() << std::endl;
                            continue;
                        }
                    }

                    write_local(cid, data, req.path, req.parameters);
                    return data;
                } catch (const std::exception& e) {
                    std::cerr << "[VFSNode " << config_.id << "] ERROR converting neighbor body to vector: " << e.what() << " (Path: " << req.path << ")" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] Exception during neighbor READ from " << neighbor << ": " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "[VFSNode " << config_.id << "] Unknown exception during neighbor READ from " << neighbor << std::endl;
        }
    }

    return {};
}

void VFSNode::write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) {
    std::string cid = get_cid(path, parameters);
    write_local(cid, data, path, parameters);
}

std::string VFSNode::get_cid(const std::string& path, const json& parameters) {
    json selector = {{"path", path}, {"parameters", parameters}};
    std::string serialized = selector.dump();
    return picosha2::hash256_hex_string(serialized);
}

bool VFSNode::has_local(const std::string& cid) {
    if (config_.storage_dir.empty()) return false;
    return fs::exists(fs::path(config_.storage_dir) / (cid + ".data"));
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    fs::path p = fs::path(config_.storage_dir) / (cid + ".data");
    if (!fs::exists(p)) return {};

    try {
        std::ifstream is(p, std::ios::binary | std::ios::ate);
        if (!is) {
            std::cerr << "[VFSNode " << config_.id << "] FAILED to open local file for reading: " << p << std::endl;
            return {};
        }

        std::streamsize size = is.tellg();
        if (size < 0) {
            std::cerr << "[VFSNode " << config_.id << "] FAILED to get size for " << p << std::endl;
            return {};
        }
        if (size == 0) return {};
        
        if (size > (std::streamsize)1024 * 1024 * 1024) {
             std::cerr << "[VFSNode " << config_.id << "] REJECTED read of " << p << ": Size " << size << " exceeds 1GB limit" << std::endl;
             return {};
        }

        is.seekg(0, std::ios::beg);
        std::vector<uint8_t> buffer;
        try {
            buffer.resize((size_t)size);
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] CRITICAL: Failed to allocate buffer of size " << size << " for " << p << ": " << e.what() << std::endl;
            return {};
        }

        if (is.read((char*)buffer.data(), size)) {
            return buffer;
        } else {
            std::cerr << "[VFSNode " << config_.id << "] FAILED to read content from " << p << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[VFSNode " << config_.id << "] Error reading local file " << p << ": " << e.what() << std::endl;
    }

    return {};
}

void VFSNode::write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params) {
    if (config_.storage_dir.empty()) return;
    
    fs::path data_path = fs::path(config_.storage_dir) / (cid + ".data");
    fs::path meta_path = fs::path(config_.storage_dir) / (cid + ".meta");

    std::ofstream os(data_path, std::ios::binary);
    os.write((const char*)data.data(), data.size());

    std::ofstream mos(meta_path);
    mos << json({{"path", path}, {"parameters", params}}).dump(2);
}

} // namespace fs
} // namespace jotcad
