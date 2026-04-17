#include "../include/vfs_node.h"
#include "../vendor/httplib.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <sstream>
#include <iomanip>
#include <condition_variable>
#include <queue>

namespace jotcad {
namespace fs {

static std::string vfs_hash256(const std::vector<uint8_t>& data) {
    uint64_t hash = 0x811c9dc5;
    for (auto b : data) { hash ^= b; hash *= 0x01000193; }
    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

static std::string vfs_hash256_str(const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    return vfs_hash256(data);
}

static void normalize_json(json& j) {
    if (j.is_object()) {
        json sorted = json::object();
        for (auto it = j.begin(); it != j.end(); ++it) {
            json val = it.value();
            normalize_json(val);
            sorted[it.key()] = val;
        }
        j = sorted;
    } else if (j.is_array()) {
        for (auto& item : j) normalize_json(item);
    }
}

static bool matches(const json& selector, const json& target) {
    if (selector.contains("path") && target.contains("path")) {
        std::string s_path = selector["path"];
        std::string t_path = target["path"];
        if (s_path.back() == '*') {
            if (t_path.find(s_path.substr(0, s_path.size()-1)) != 0) return false;
        } else if (s_path != t_path) return false;
    }
    if (selector.contains("parameters") && target.contains("parameters")) {
        for (auto& [key, value] : selector["parameters"].items()) {
            if (!target["parameters"].contains(key) || target["parameters"][key] != value) return false;
        }
    }
    return true;
}

struct Command {
    std::string type;
    std::string id;
    std::string path;
    json parameters;
    std::vector<std::string> stack;
    long long expiresAt;
    json payload;
};

struct PeerQueue {
    std::queue<Command> commands;
    std::mutex mutex;
    std::condition_variable cv;
};

static std::map<std::string, std::shared_ptr<PeerQueue>> g_reverse_queues;
static std::mutex g_reverse_queues_mutex;

static std::shared_ptr<PeerQueue> get_or_create_queue(const std::string& peer_id) {
    std::lock_guard<std::mutex> lock(g_reverse_queues_mutex);
    if (!g_reverse_queues.count(peer_id)) {
        g_reverse_queues[peer_id] = std::make_shared<PeerQueue>();
    }
    return g_reverse_queues[peer_id];
}

VFSNode::VFSNode(const Config& config) : config_(config) {
    server_ptr_ = new httplib::Server();
    if (!config_.storage_dir.empty()) {
        std::filesystem::remove_all(config_.storage_dir);
        std::filesystem::create_directories(config_.storage_dir);
    }
}

VFSNode::~VFSNode() {
    stop();
    delete static_cast<httplib::Server*>(server_ptr_);
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFSNode::notify_schema() {
    json catalog = get_catalog();
    notify({{"path", "sys/schema"}}, catalog);
}

json VFSNode::get_catalog() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    json catalog = json::object();
    for (auto const& [path, schema] : schemas_) {
        catalog[path] = schema;
    }
    return {{"catalog", catalog}, {"id", config_.id}};
}

json VFSNode::get_neighbors() {
    std::lock_guard<std::mutex> lock(peer_mutex_);
    json neighbors = json::array();
    for (auto const& [id, url] : peers_) {
        neighbors.push_back({{"id", id}, {"url", url}, {"reachability", "DIRECT"}});
    }
    return neighbors;
}

void VFSNode::add_peer(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (connecting_.count(url)) return;
        connecting_.insert(url);
    }
    httplib::Client cli(url);
    cli.set_connection_timeout(std::chrono::seconds(1));
    if (auto res = cli.Get("/health")) {
        if (res->status == 200) {
            try {
                json body = json::parse(res->body);
                std::string id = body["id"];
                {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    peers_[id] = url;
                }
                std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << " (" << url << ")" << std::endl;
                // Notify mesh of topology change
                notify({{"path", "sys/topo"}}, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}});
            } catch (...) {}
        }
    }
    { std::lock_guard<std::mutex> lock(peer_mutex_); connecting_.erase(url); }
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
            auto data = read_impl(vfs_req);
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            res.status = e.code;
            res.set_content(json({{"type", "sys/error"}, {"message", e.what()}, {"code", e.code}}).dump(), "application/json");
        } catch (...) { res.status = 500; }
    });
    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) { res.status = 400; return; }
        auto q = get_or_create_queue(peer_id);
        std::unique_lock<std::mutex> lock(q->mutex);
        if (q->cv.wait_for(lock, std::chrono::seconds(30), [&q] { return !q->commands.empty(); })) {
            Command cmd = q->commands.front(); q->commands.pop();
            json resp = {{"type", cmd.type}, {"op", cmd.type == "COMMAND" ? "READ" : "NOTIFY"}, {"id", cmd.id}};
            if (cmd.type == "COMMAND") { resp["path"] = cmd.path; resp["parameters"] = cmd.parameters; resp["stack"] = cmd.stack; resp["expiresAt"] = cmd.expiresAt; }
            else { resp["selector"] = cmd.parameters; resp["payload"] = cmd.payload; }
            res.set_content(resp.dump(), "application/json");
        } else { res.status = 204; }
    });
    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            json selector = body.at("selector");
            long long expiresAt = body.at("expiresAt").get<long long>();
            std::vector<std::string> stack = body.value("stack", std::vector<std::string>());
            std::string peer_id = req.get_header_value("x-vfs-id");
            if (peer_id.empty() && !stack.empty()) peer_id = stack.back();
            if (!peer_id.empty()) {
                std::lock_guard<std::mutex> lock(interest_mutex_);
                std::string key = selector.dump();
                interests_[key][peer_id] = expiresAt;
                interest_selectors_[key] = selector;
                
                // Immediate state push for Topology/Schema subscriptions (Detached)
                if (selector["path"] == "sys/topo" || selector["path"] == "sys/schema") {
                    std::thread([this, selector, key, peer_id]() {
                        // Small delay to ensure caller has finished processing the SUB response
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                        if (selector["path"] == "sys/topo") {
                            notify(selector, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}}, {});
                        } else {
                            notify(selector, {{"type", "CATALOG_ANNOUNCEMENT"}, {"provider", config_.id}, {"catalog", get_catalog()["catalog"]}}, {});
                        }
                    }).detach();
                }
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
    svr->Post("/register", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string peer_url = body.value("url", "");
            if (!peer_url.empty()) std::thread([this, peer_url]() { this->add_peer(peer_url); }).detach();
            res.set_content(json({{"id", config_.id}}).dump(), "application/json");
        } catch (...) { res.status = 400; }
    });
    std::cout << "[VFSNode " << config_.id << "] Listening on 0.0.0.0:" << config_.port << std::endl;
    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);
    if (svr->is_running()) svr->stop();
}

bool VFSNode::validate_selector(const VFSRequest& req, std::string& error_out) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (!schemas_.count(req.path)) return true;
    const json& schema = schemas_[req.path];
    if (schema.contains("required")) {
        for (const auto& f : schema["required"]) { if (!req.parameters.contains(f.get<std::string>())) { error_out = "Missing required parameter: " + f.get<std::string>(); return false; } }
    }
    return true;
}

std::vector<uint8_t> VFSNode::read_impl(const VFSRequest& req, int depth) {
    std::cout << "[VFS " << config_.id << "] READ: " << req.path << " " << req.parameters.dump() << " (depth: " << depth << ")" << std::endl;
    if (depth > 10) throw VFSException("Recursive link depth exceeded", 508);
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    if (req.expiresAt > 0 && now > req.expiresAt) throw VFSException("VFS Request Expired", 408);
    if (req.stack.size() > 10) throw VFSException("VFS Routing Loop or Stack Depth exceeded", 400);
    std::string error; if (!validate_selector(req, error)) throw VFSException(error, 400);
    std::string cid = get_cid(req.path, req.parameters);
    if (has_local(cid)) {
        try {
            json meta; { std::lock_guard<std::mutex> lock(storage_mutex_); std::ifstream is((std::filesystem::path(config_.storage_dir) / (cid + ".meta")).string()); meta = json::parse(is); }
            if (meta.value("state", "") == "LINKED" && meta.contains("target")) {
                VFSRequest next_req = req; next_req.path = meta["target"]["path"]; next_req.parameters = meta["target"]["parameters"];
                return read_impl(next_req, depth + 1);
            }
        } catch (...) {}
    }
    if (has_local(cid)) return get_local(cid);
    OpHandler handler = nullptr; { std::lock_guard<std::mutex> lock(handlers_mutex_); if (handlers_.count(req.path)) handler = handlers_[req.path]; }
    if (handler) {
        auto data = handler(req);
        if (!data.empty()) { write_local(cid, data, req.path, req.parameters); return data; }
    }
    std::vector<std::string> new_stack = req.stack; new_stack.push_back(config_.id);
    std::vector<std::string> target_urls; { std::lock_guard<std::mutex> lock(peer_mutex_); for (const auto& [id, url] : peers_) { bool in_stack = false; for (const auto& s_id : req.stack) if (s_id == id) in_stack = true; if (!in_stack) target_urls.push_back(url); } }
    for (const auto& url : target_urls) {
        try {
            httplib::Client cli(url); cli.set_read_timeout(60, 0);
            json body = {{"path", req.path}, {"parameters", req.parameters}, {"stack", new_stack}, {"expiresAt", req.expiresAt}};
            auto res = cli.Post("/read", body.dump(), "application/json");
            if (res && res->status == 200) {
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                write_local(cid, data, req.path, req.parameters); return data;
            } else if (res && res->status >= 400 && res->status != 404) {
                json err_body = json::parse(res->body); throw VFSException(err_body.value("message", "Upstream peer error"), res->status);
            }
        } catch (const VFSException&) { throw; } catch (...) {}
    }
    throw VFSException("Artifact not found in mesh: " + req.path, 404);
}

void VFSNode::write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) {
    std::cout << "[VFS " << config_.id << "] WRITE: " << path << " " << parameters.dump() << " (" << data.size() << " bytes)" << std::endl;
    write_local(get_cid(path, parameters), data, path, parameters);
}

std::string VFSNode::write_cid(const std::string& path, const std::vector<uint8_t>& data) {
    std::string hash = vfs_hash256(data);
    json parameters = {{"cid", hash}};
    std::cout << "[VFS " << config_.id << "] WRITE_CID: " << path << " cid=" << hash << std::endl;
    write_local(get_cid(path, parameters), data, path, parameters);
    return hash;
}

void VFSNode::link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) {
    write_local_link(get_cid(src_path, src_params), src_path, src_params, tgt_path, tgt_params);
}

std::string VFSNode::get_cid(const std::string& path, const json& parameters) {
    json s = {{"path", path}, {"parameters", parameters}};
    normalize_json(s);
    return vfs_hash256_str(s.dump());
}

bool VFSNode::has_local(const std::string& cid) {
    if (config_.storage_dir.empty()) return false;
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return std::filesystem::exists(std::filesystem::path(config_.storage_dir) / (cid + ".data"));
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::ifstream is(p, std::ios::binary | std::ios::ate);
    if (!is) return {};
    std::streamsize size = is.tellg(); is.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf((size_t)size); if (is.read((char*)buf.data(), (size_t)size)) return buf;
    return {};
}

void VFSNode::write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params) {
    if (config_.storage_dir.empty()) return;
    std::cout << "[VFS " << config_.id << "] Persisting local CID: " << cid << " for " << path << std::endl;
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::ofstream os(std::filesystem::path(config_.storage_dir) / (cid + ".data"), std::ios::binary);
    os.write((const char*)data.data(), data.size());
    std::ofstream mos(std::filesystem::path(config_.storage_dir) / (cid + ".meta"));
    mos << json({{"path", path}, {"parameters", params}, {"state", "AVAILABLE"}}).dump();
}

void VFSNode::write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) {
    if (config_.storage_dir.empty()) return;
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::ofstream os(std::filesystem::path(config_.storage_dir) / (src_cid + ".data"));
    os << "vfs:/" << tgt_path;
    std::ofstream mos(std::filesystem::path(config_.storage_dir) / (src_cid + ".meta"));
    mos << json({ {"path", src_path}, {"parameters", src_params}, {"state", "LINKED"}, {"target", {{"path", tgt_path}, {"parameters", tgt_params}}} }).dump();
}

std::vector<uint8_t> VFSNode::spy(const VFSRequest& req) {
    std::vector<uint8_t> res; if (!std::filesystem::exists(config_.storage_dir)) return res;
    for (const auto& entry : std::filesystem::directory_iterator(config_.storage_dir)) {
        if (entry.path().extension() == ".meta") {
            try {
                std::ifstream is(entry.path()); json meta = json::parse(is);
                if (meta["path"].get<std::string>().find(req.path) != std::string::npos) {
                    auto data = get_local(entry.path().stem().string());
                    std::string header = "\n=" + std::to_string(data.size()) + " " + meta.dump() + "\n";
                    res.insert(res.end(), header.begin(), header.end()); res.insert(res.end(), data.begin(), data.end());
                }
            } catch (...) {}
        }
    }
    return res;
}

void VFSNode::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    std::vector<std::string> n_stack = stack; n_stack.push_back(config_.id);
    std::lock_guard<std::mutex> lock(peer_mutex_);
    for (const auto& [id, url] : peers_) {
        bool skip = false; for (const auto& s : stack) if (s == id) skip = true;
        if (!skip) std::thread([url, selector, expiresAt, n_stack]() {
            httplib::Client cli(url); cli.set_connection_timeout(std::chrono::seconds(1));
            cli.Post("/subscribe", json({{"selector", selector}, {"expiresAt", expiresAt}, {"stack", n_stack}}).dump(), "application/json");
        }).detach();
    }
}

void VFSNode::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    // 1. Inbound Rule: If we have already seen this message, drop it.
    for (const auto& id : stack) if (id == config_.id) return;

    // 2. Relay Rule: Prepare the stack for the next hop.
    std::vector<std::string> next_stack = stack; 
    next_stack.push_back(config_.id);

    std::lock_guard<std::mutex> lock(interest_mutex_);
    for (auto& [key, subs] : interests_) {
        if (matches(interest_selectors_[key], selector)) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for (auto it = subs.begin(); it != subs.end(); ) {
                if (now > it->second) { it = subs.erase(it); continue; }
                std::string p_id = it->first; 
                
                // 3. Outbound Rule: If the neighbor has already seen this message, skip them.
                bool neighbor_seen = false;
                for (const auto& s_id : stack) if (s_id == p_id) neighbor_seen = true;
                if (neighbor_seen) { ++it; continue; }

                bool sent = false;
                { 
                    std::lock_guard<std::mutex> ql(g_reverse_queues_mutex);
                    if (g_reverse_queues.count(p_id)) {
                        auto q = g_reverse_queues[p_id]; std::lock_guard<std::mutex> lq(q->mutex);
                        q->commands.push({"NOTIFY", "0", "", selector, {}, 0, payload});
                        q->cv.notify_one(); sent = true;
                    }
                }
                if (!sent) {
                    std::string p_url; { std::lock_guard<std::mutex> pl(peer_mutex_); if (peers_.count(p_id)) p_url = peers_[p_id]; }
                    if (!p_url.empty()) std::thread([p_url, selector, payload, next_stack]() {
                        httplib::Client cli(p_url); cli.set_connection_timeout(std::chrono::seconds(1));
                        cli.Post("/notify", json({{"selector", selector}, {"payload", payload}, {"stack", next_stack}}).dump(), "application/json");
                    }).detach();
                }
                ++it;
            }
        }
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {}

} // namespace fs
} // namespace jotcad
