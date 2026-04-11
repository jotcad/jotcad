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

void VFSNode::register_op(const std::string& path, OpHandler handler) {
    handlers_[path] = handler;
}

void VFSNode::listen() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);

    svr->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"status", "OK"}, {"id", config_.id}}).dump(), "application/json");
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            VFSRequest vfs_req;

            vfs_req.path = body.at("path").get<std::string>();
            vfs_req.parameters = body.value("parameters", json::object());
            
            if (body.contains("stack")) {
                for (auto& item : body["stack"]) vfs_req.stack.push_back(item.get<std::string>());
            }

            // Loop Prevention
            for (const auto& s_id : vfs_req.stack) {
                if (s_id == config_.id) {
                    res.status = 404;
                    res.set_content("Loop Detected", "text/plain");
                    return;
                }
            }

            // Expiration Check
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            if (body.contains("expiresAt")) {
                vfs_req.expiresAt = body["expiresAt"].get<long long>();
                if (now > vfs_req.expiresAt) {
                    res.status = 408;
                    res.set_content("Request Expired", "text/plain");
                    return;
                }
            } else {
                vfs_req.expiresAt = now + 30000; // Default 30s
            }

            auto data = read(vfs_req);
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

    std::cout << "[VFSNode " << config_.id << "] Listening on 0.0.0.0:" << config_.port << std::endl;
    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);
    if (svr->is_running()) svr->stop();
}

std::vector<uint8_t> VFSNode::read(const VFSRequest& req) {
    std::string cid = get_cid(req.path, req.parameters);

    if (has_local(cid)) return get_local(cid);

    if (handlers_.count(req.path)) {
        auto data = handlers_[req.path](req);
        if (!data.empty()) {
            write_local(cid, data, req.path, req.parameters);
            return data;
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
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                write_local(cid, data, req.path, req.parameters);
                return data;
            }
        } catch (...) {}
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
    std::ifstream is(fs::path(config_.storage_dir) / (cid + ".data"), std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
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
