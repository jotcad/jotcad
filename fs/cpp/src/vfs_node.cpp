#include "../include/vfs_node.h"
#include "../vendor/httplib.h"
#include "../vendor/sha256.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>

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
    return std::vector<uint8_t>(s.begin(), s.end());
}

bool matches(const json& sub, const json& event) {
    if (sub["path"] == event["path"]) {
        if (sub["parameters"].empty()) return true;
        return sub["parameters"] == event["parameters"];
    }
    return false;
}
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    handlers_[path] = handler;
    if (!schema.empty()) {
        schemas_[path] = schema;
        std::string serialized = schema.dump();
        write("sys/schema", {{"target", path}}, safe_to_bytes(serialized));
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {
    std::lock_guard<std::mutex> lock(reverse_mutex_);
    reverse_listeners_[peer_id] = &res;
}

void VFSNode::add_peer(const std::string& url) {
    if (url.empty()) return;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (connecting_.count(url)) return;
        connecting_.insert(url);
    }

    try {
        httplib::Client cli(url);
        cli.set_connection_timeout(std::chrono::seconds(2));
        json body = {{"id", config_.id}, {"url", "http://localhost:" + std::to_string(config_.port)}};
        if (auto res = cli.Post("/register", body.dump(), "application/json")) {
            if (res->status == 200) {
                auto info = json::parse(res->body);
                std::string remote_id = info.at("id");
                if (!remote_id.empty() && remote_id != config_.id) {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    peers_[remote_id] = url;
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
        try {
            auto body = json::parse(req.body);
            VFSRequest vfs_req;
            vfs_req.path = body.at("path").get<std::string>();
            vfs_req.parameters = body.value("parameters", json::object());
            vfs_req.stack = body.value("stack", std::vector<std::string>());
            vfs_req.expiresAt = body.value("expiresAt", 0LL);
            auto data = read(vfs_req);
            if (!data.empty()) {
                res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
            } else { res.status = 404; }
        } catch (...) { res.status = 400; }
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
            } else { res.status = 404; }
        } catch (...) { res.status = 400; }
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
                if (auto h_res = cli.Get("/health")) can_reach = h_res->status == 200;
            }
            if (can_reach) std::thread([this, peer_url]() { this->add_peer(peer_url); }).detach();
            res.set_content(json({{"id", config_.id}, {"reachability", can_reach ? "DIRECT" : "REVERSE"}}).dump(), "application/json");
        } catch (...) { res.status = 400; }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            json selector = body.at("selector");
            long long expiresAt = body.at("expiresAt").get<long long>();
            std::vector<std::string> stack = body.value("stack", std::vector<std::string>());
            std::string peer_id = req.get_header_value("x-vfs-id");
            if (peer_id.empty()) peer_id = "unknown";

            std::string key = selector.dump();
            {
                std::lock_guard<std::mutex> lock(interest_mutex_);
                interests_[key][peer_id] = expiresAt;
                interest_selectors_[key] = selector;
            }
            subscribe(selector, expiresAt, stack);
            res.status = 200;
        } catch (...) { res.status = 400; }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            notify(body.at("selector"), body.at("payload"), body.value("stack", std::vector<std::string>()));
            res.status = 200;
        } catch (...) { res.status = 400; }
    });

    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) { res.status = 400; return; }
        register_reverse_peer(peer_id, res);
    });

    for (const auto& url : config_.neighbors) std::thread([this, url]() { this->add_peer(url); }).detach();

    // Topology Heartbeat Thread
    std::thread([this]() {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            json neighbors = json::array();
            {
                std::lock_guard<std::mutex> lock(peer_mutex_);
                for (const auto& [id, url] : peers_) neighbors.push_back({{"id", id}, {"reachability", "DIRECT"}});
            }
            notify({{"path", "sys/topo"}, {"parameters", {{"id", config_.id}}}}, {
                {"type", "TOPOLOGY_UPDATE"},
                {"peer", config_.id},
                {"neighbors", neighbors}
            });
        }
    }).detach();

    std::cout << "[VFSNode " << config_.id << "] Listening on 0.0.0.0:" << config_.port << std::endl;
    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);
    if (svr->is_running()) svr->stop();
}

bool VFSNode::validate_selector(const VFSRequest& req, std::string& error_out) {
    if (!schemas_.count(req.path)) return true;
    const json& schema = schemas_[req.path];
    if (schema.contains("required")) {
        for (const auto& f : schema["required"]) {
            if (!req.parameters.contains(f.get<std::string>())) {
                error_out = "Missing: " + f.get<std::string>();
                return false;
            }
        }
    }
    return true;
}

std::vector<uint8_t> VFSNode::read(const VFSRequest& req) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (req.expiresAt > 0 && now > req.expiresAt) return {};
    std::string error;
    if (!validate_selector(req, error)) return {};

    std::string cid = get_cid(req.path, req.parameters);
    if (has_local(cid)) return get_local(cid);

    std::vector<uint8_t> data;
    if (handlers_.count(req.path)) {
        json selector = {{"path", req.path}, {"parameters", req.parameters}};
        notify(selector, {{"type", "WORKING"}, {"peer", config_.id}});
        data = handlers_[req.path](req);
        if (!data.empty()) {
            notify(selector, {{"type", "SUCCESS"}, {"peer", config_.id}, {"size", data.size()}});
            write_local(cid, data, req.path, req.parameters);
        }
    }

    if (data.empty()) {
        std::vector<std::string> new_stack = req.stack;
        new_stack.push_back(config_.id);
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (const auto& [id, url] : peers_) {
            bool in_stack = false; for (const auto& s_id : req.stack) if (s_id == id) in_stack = true;
            if (in_stack) continue;
            try {
                httplib::Client cli(url); cli.set_read_timeout(30, 0);
                json body = {{"path", req.path}, {"parameters", req.parameters}, {"stack", new_stack}, {"expiresAt", req.expiresAt}};
                auto res = cli.Post("/read", body.dump(), "application/json");
                if (res && res->status == 200) {
                    data = std::vector<uint8_t>(res->body.begin(), res->body.end());
                    write_local(cid, data, req.path, req.parameters);
                    break;
                }
            } catch (...) {}
        }
    }

    if (!data.empty()) {
        try {
            auto j = json::parse(std::string(data.begin(), data.end()));
            if (j.contains("geometry") && j["geometry"].is_string()) {
                std::string url = j["geometry"];
                if (url.compare(0, 5, "vfs:/") == 0) {
                    VFSRequest link_req;
                    link_req.path = url.substr(5);
                    link_req.parameters = j.value("parameters", req.parameters);
                    link_req.stack = req.stack;
                    link_req.expiresAt = req.expiresAt;
                    return read(link_req);
                }
            }
        } catch (...) {}
    }
    return data;
}

void VFSNode::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    std::vector<std::string> n_stack = stack; n_stack.push_back(config_.id);
    std::lock_guard<std::mutex> lock(peer_mutex_);
    for (const auto& [id, url] : peers_) {
        bool skip = false; for (const auto& s : stack) if (s == id) skip = true;
        if (skip) continue;
        std::thread([url, selector, expiresAt, n_stack, this]() {
            httplib::Client cli(url); cli.set_connection_timeout(std::chrono::seconds(1));
            json b = {{"selector", selector}, {"expiresAt", expiresAt}, {"stack", n_stack}};
            cli.Post("/subscribe", b.dump(), "application/json");
        }).detach();
    }
}

void VFSNode::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    for (const auto& id : stack) if (id == config_.id) return;
    std::vector<std::string> n_stack = stack; n_stack.push_back(config_.id);

    std::lock_guard<std::mutex> lock(interest_mutex_);
    for (auto& [key, subs] : interests_) {
        if (matches(interest_selectors_[key], selector)) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for (auto it = subs.begin(); it != subs.end(); ) {
                if (now > it->second) { it = subs.erase(it); continue; }
                std::string p_id = it->first, p_url;
                { std::lock_guard<std::mutex> pl(peer_mutex_); if (peers_.count(p_id)) p_url = peers_[p_id]; }
                
                // Don't send back to neighbor that sent it
                bool in_stack = false;
                for (const auto& s_id : stack) if (s_id == p_id) in_stack = true;

                if (!p_url.empty() && !in_stack) std::thread([p_url, selector, payload, n_stack]() {
                    httplib::Client cli(p_url); cli.set_connection_timeout(std::chrono::seconds(1));
                    json b = {{"selector", selector}, {"payload", payload}, {"stack", n_stack}};
                    cli.Post("/notify", b.dump(), "application/json");
                }).detach();
                ++it;
            }
        }
    }
}

std::vector<uint8_t> VFSNode::spy(const VFSRequest& req) {
    std::vector<uint8_t> res;
    if (fs::exists(config_.storage_dir)) {
        for (const auto& entry : fs::directory_iterator(config_.storage_dir)) {
            if (entry.path().extension() == ".meta") {
                try {
                    std::ifstream is(entry.path());
                    json meta = json::parse(is);
                    if (meta["path"].get<std::string>().find(req.path) != std::string::npos) {
                        auto data = get_local(entry.path().stem().string());
                        if (data.empty()) continue;
                        std::string header = "\n=" + std::to_string(data.size()) + " " + meta.dump() + "\n";
                        res.insert(res.end(), header.begin(), header.end());
                        res.insert(res.end(), data.begin(), data.end());
                    }
                } catch (...) {}
            }
        }
    }
    return res;
}

void VFSNode::write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) {
    write_local(get_cid(path, parameters), data, path, parameters);
}

std::string VFSNode::get_cid(const std::string& path, const json& parameters) {
    json s = {{"path", path}, {"parameters", parameters}};
    return picosha2::hash256_hex_string(s.dump());
}

bool VFSNode::has_local(const std::string& cid) {
    return !config_.storage_dir.empty() && fs::exists(fs::path(config_.storage_dir) / (cid + ".data"));
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    fs::path p = fs::path(config_.storage_dir) / (cid + ".data");
    std::ifstream is(p, std::ios::binary | std::ios::ate);
    if (!is) return {};
    std::streamsize size = is.tellg();
    if (size <= 0) return {};
    is.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf((size_t)size);
    if (is.read((char*)buf.data(), (size_t)size)) return buf;
    return {};
}

void VFSNode::write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params) {
    if (config_.storage_dir.empty()) return;
    std::ofstream os(fs::path(config_.storage_dir) / (cid + ".data"), std::ios::binary);
    os.write((const char*)data.data(), data.size());
    std::ofstream mos(fs::path(config_.storage_dir) / (cid + ".meta"));
    mos << json({{"path", path}, {"parameters", params}}).dump(2);
}

} // namespace fs
} // namespace jotcad
