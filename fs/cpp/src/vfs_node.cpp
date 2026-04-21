#include "../include/vfs_node.h"
#include "../include/cid.h"
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

namespace fs {

static void normalize_json(json& j) {
    if (j.is_object()) {
        json sorted = json::object();
        std::map<std::string, json> m;
        for (auto it = j.begin(); it != j.end(); ++it) m[it.key()] = it.value();
        for (auto const& [k, v] : m) {
            json val = v;
            normalize_json(val);
            sorted[k] = val;
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
        if (s_path.size() > 0 && s_path.back() == '*') {
            if (t_path.find(s_path.substr(0, s_path.size()-1)) != 0) return false;
        } else if (s_path != t_path) return false;
    }
    if (selector.contains("parameters") && selector["parameters"].is_object()) {
        for (auto& [key, value] : selector["parameters"].items()) {
            if (!target.contains("parameters") || !target["parameters"].contains(key) || target["parameters"][key] != value) {
                return false;
            }
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
    notify({{"path", "sys/schema"}}, catalog, {});
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
                notify({{"path", "sys/topo"}}, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}}, {});
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
            Selector sel = body.at("selector").get<Selector>();
            
            std::cout << "[VFSNode " << config_.id << "] /read request for " << sel.path << std::endl;

            VFSRequest vfs_req;
            vfs_req.selector = sel;
            vfs_req.stack = body.value("stack", std::vector<std::string>());
            vfs_req.expiresAt = body.value("expiresAt", 0LL);

            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            if (vfs_req.expiresAt > 0 && now > vfs_req.expiresAt) {
                throw VFSException("VFS Request Expired", 404);
            }

            auto data = read_impl(vfs_req.selector);
            res.status = 200;
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            res.status = e.code;
            res.set_content(json({{"type", "sys/error"}, {"message", e.what()}, {"code", e.code}}).dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(json({{"type", "sys/error"}, {"message", e.what()}, {"code", 500}}).dump(), "application/json");
        } catch (...) { 
            res.status = 500; 
            res.set_content(json({{"type", "sys/error"}, {"message", "Unknown internal error"}, {"code", 500}}).dump(), "application/json");
        }
    });
    svr->Post("/listen", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peer_id = req.get_header_value("x-vfs-peer-id");
        if (peer_id.empty()) { res.status = 400; return; }
        auto q = get_or_create_queue(peer_id);
        std::unique_lock<std::mutex> lock(q->mutex);
        if (q->cv.wait_for(lock, std::chrono::seconds(30), [&q] { return !q->commands.empty(); })) {
            Command cmd = q->commands.front(); q->commands.pop();
            json resp = {{"type", cmd.type}, {"op", cmd.type == "COMMAND" ? (cmd.id.empty() ? "SUB" : "READ") : "NOTIFY"}, {"id", cmd.id}};
            if (cmd.type == "COMMAND") { resp["selector"] = {{"path", cmd.path}, {"parameters", cmd.parameters}}; resp["stack"] = cmd.stack; resp["expiresAt"] = cmd.expiresAt; }
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
            if (peer_id.empty()) peer_id = "anonymous-neighbor-" + req.remote_addr;
            {
                std::lock_guard<std::mutex> lock(interest_mutex_);
                std::string key = selector.dump();
                interests_[key][peer_id] = expiresAt;
                interest_selectors_[key] = selector;
                std::string remote_url = req.get_header_value("x-vfs-local-url");
                if (!remote_url.empty() && !peers_.count(peer_id)) {
                    std::lock_guard<std::mutex> pl(peer_mutex_);
                    peers_[peer_id] = remote_url;
                }
            }
            if (selector["path"] == "sys/topo" || selector["path"] == "sys/schema") {
                std::thread([this, selector, peer_id]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    if (selector["path"] == "sys/topo") {
                        notify(selector, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}}, {});
                    } else {
                        notify(selector, {{"type", "CATALOG_ANNOUNCEMENT"}, {"provider", config_.id}, {"catalog", get_catalog()["catalog"]}}, {});
                    }
                }).detach();
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
            std::string remote_id = body.value("id", "");
            std::string remote_url = body.value("url", "");
            if (!remote_id.empty()) {
                std::lock_guard<std::mutex> lock(peer_mutex_);
                peers_[remote_id] = remote_url;
                if (!remote_url.empty()) {
                    std::thread([this, remote_url]() { this->add_peer(remote_url); }).detach();
                }
            }
            res.set_content(json({{"id", config_.id}}).dump(), "application/json");
        } catch (...) { res.status = 400; }
    });
    std::cout << "[VFSNode " << config_.id << "] Listening on 0.0.0.0:" << config_.port << std::endl;
    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    auto* svr = static_cast<httplib::Server*>(server_ptr_);
    if (svr && svr->is_running()) svr->stop();
}

bool VFSNode::validate_selector(const VFSRequest& req, std::string& error_out) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (!schemas_.count(req.selector.path)) return true;
    const json& schema = schemas_[req.selector.path];
    
    if (schema.contains("required")) {
        for (const auto& f : schema["required"]) {
            std::string field = f.get<std::string>();
            if (!req.selector.parameters.contains(field)) {
                error_out = "Missing required parameter '" + field + "'";
                return false;
            }
        }
    }

    if (schema.contains("properties")) {
        for (auto it = schema["properties"].begin(); it != schema["properties"].end(); ++it) {
            std::string key = it.key();
            if (req.selector.parameters.contains(key)) {
                const auto& prop = it.value();
                const auto& val = req.selector.parameters[key];
                if (prop.value("type", "") == "number" && !val.is_number()) {
                    error_out = "Invalid parameter type for '" + key + "': Expected number";
                    return false;
                }
                if (prop.value("type", "") == "string" && !val.is_string()) {
                    error_out = "Invalid parameter type for '" + key + "': Expected string";
                    return false;
                }
                if (prop.contains("enum")) {
                    bool found = false;
                    for (const auto& e : prop["enum"]) { if (e == val) found = true; }
                    if (!found) {
                        error_out = "Invalid enum value for '" + key + "'";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

std::vector<uint8_t> VFSNode::read_impl(const Selector& sel, int depth, std::vector<std::string> stack) {
    if (sel.path.empty() && !sel.parameters.contains("cid")) throw VFSException("Selector must have a path or CID", 400);
    if (depth > 20) throw VFSException("Recursive link depth exceeded", 508);
    
    // 1. Direct CID Check
    if (sel.parameters.contains("cid")) {
        std::string cid = sel.parameters["cid"];
        if (has_local(cid)) return get_local(cid);
    }

    // 2. Address Lookup
    std::string addrKey = get_cid(sel);
    if (has_local(addrKey)) {
        try {
            json meta; { std::lock_guard<std::mutex> lock(storage_mutex_); std::ifstream is((std::filesystem::path(config_.storage_dir) / (addrKey + ".meta")).string()); meta = json::parse(is); }
            if (meta.value("state", "") == "LINKED" && meta.contains("target")) {
                Selector next_sel = Selector::from_json(meta["target"]);
                return read_impl(next_sel, depth + 1, stack);
            }
            if (meta.contains("cid")) return get_local(meta["cid"]);
        } catch (...) {}
    }

    OpHandler handler = nullptr; { std::lock_guard<std::mutex> lock(handlers_mutex_); if (handlers_.count(sel.path)) handler = handlers_[sel.path]; }
    if (handler) {
        VFSRequest req = {sel, {config_.id}, 0};
        auto data = handler(req);
        if (!data.empty()) {
            write_bytes(sel, data);
            return data;
        }
        if (has_local(addrKey)) {
             json meta; { std::lock_guard<std::mutex> lock(storage_mutex_); std::ifstream is((std::filesystem::path(config_.storage_dir) / (addrKey + ".meta")).string()); meta = json::parse(is); }
             if (meta.contains("cid")) return get_local(meta["cid"]);
        }
    }
    
    // 3. Mesh Scan (Mesh-Routing Protection using Node IDs in stack)
    for (const auto& id : stack) if (id == config_.id) return {};
    std::vector<std::string> next_stack = stack; next_stack.push_back(config_.id);

    std::vector<std::string> target_urls; 
    { std::lock_guard<std::mutex> lock(peer_mutex_); for (const auto& [id, url] : peers_) {
        bool seen = false;
        for (const auto& sid : stack) if (sid == id) seen = true;
        if (!seen) target_urls.push_back(url);
    }}

    for (const auto& url : target_urls) {
        try {
            httplib::Client cli(url); cli.set_read_timeout(5, 0);
            json body = {{"selector", sel}, {"stack", next_stack}};
            auto res = cli.Post("/read", body.dump(), "application/json");
            if (res && res->status == 200) {
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                write_bytes(sel, data); 
                return data;
            }
        } catch (...) {}
    }

    throw VFSException("Artifact not found in mesh: " + sel.path, 404);
}

void VFSNode::link(const Selector& src, const Selector& tgt) {
    std::string srcKey = get_cid(src);
    write_local_link(srcKey, src.path, src.parameters, tgt.path, tgt.parameters);
}

std::string VFSNode::get_cid(const Selector& sel) {
    json s = {{"parameters", sel.parameters}, {"path", sel.path}};
    normalize_json(s);
    return vfs_hash256_str(encode_safe(s));
}

bool VFSNode::has_local(const std::string& cid) {
    if (config_.storage_dir.empty()) return false;
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return std::filesystem::exists(std::filesystem::path(config_.storage_dir) / (cid + ".data")) ||
           std::filesystem::exists(std::filesystem::path(config_.storage_dir) / (cid + ".meta"));
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
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
                if (meta["path"].get<std::string>().find(req.selector.path) != std::string::npos) {
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
    for (const auto& id : stack) if (id == config_.id) return;
    std::vector<std::string> next_stack = stack; next_stack.push_back(config_.id);
    std::lock_guard<std::mutex> lock(interest_mutex_);
    for (auto& [key, subs] : interests_) {
        if (matches(interest_selectors_[key], selector)) {
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for (auto it = subs.begin(); it != subs.end(); ) {
                if (now > it->second) { it = subs.erase(it); continue; }
                std::string p_id = it->first; 
                bool neighbor_seen = false;
                for (const auto& s_id : stack) if (s_id == p_id) neighbor_seen = true;
                if (neighbor_seen) { ++it; continue; }
                std::string p_url; { std::lock_guard<std::mutex> pl(peer_mutex_); if (peers_.count(p_id)) p_url = peers_[p_id]; }
                if (!p_url.empty()) std::thread([p_url, selector, payload, next_stack]() {
                    httplib::Client cli(p_url); cli.set_connection_timeout(std::chrono::seconds(1));
                    cli.Post("/notify", json({{"selector", selector}, {"payload", payload}, {"stack", next_stack}}).dump(), "application/json");
                }).detach();
                ++it;
            }
        }
    }
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data) {
    std::string cid = vfs_hash256(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());
    }
    if (!sel.path.empty()) {
        std::string addrKey = get_cid(sel);
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream ms(std::filesystem::path(config_.storage_dir) / (addrKey + ".meta"));
        ms << json({{"path", sel.path}, {"parameters", sel.parameters}, {"cid", cid}, {"state", "AVAILABLE"}}).dump();
    }
    Selector out = sel;
    out.parameters["cid"] = cid;
    return out;
}

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel) {
    return read_impl(sel);
}

template<> json VFSNode::read<json>(const Selector& sel) {
    auto data = read_impl(sel);
    std::string text(data.begin(), data.end());
    try {
        return decode_safe(text);
    } catch (...) {
        return json::parse(text);
    }
}

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data) { return write_bytes(sel, data); }
template<> Selector VFSNode::write<json>(const Selector& sel, const json& data) {
    std::string raw_json = data.dump();
    std::vector<uint8_t> bytes(raw_json.begin(), raw_json.end());
    // write_bytes will automatically hash the JCB binary form for the CID
    return write_bytes(sel, bytes);
}
template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data) {
    std::vector<uint8_t> bytes(data.begin(), data.end());
    return write_bytes(sel, bytes);
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {}

} // namespace fs
