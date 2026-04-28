#include "vfs_node.h"
#include "cid.h"
#include "vendor/httplib.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <sstream>

namespace fs {

VFSNode::VFSNode(const Config& config) : config_(config), server_ptr_(nullptr) {
    if (config_.storage_dir.empty()) {
        config_.storage_dir = ".vfs_storage_" + config_.id;
    }
    std::filesystem::create_directories(config_.storage_dir);
}

VFSNode::~VFSNode() {
    stop();
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFSNode::notify_schema() {
}

void VFSNode::listen() {
    auto svr = new httplib::Server();
    server_ptr_ = svr;

    svr->set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type, X-Requested-With, X-VFS-Peer-Id, X-VFS-Reply-To, X-VFS-Id"},
        {"Access-Control-Expose-Headers", "X-VFS-Info, X-VFS-Id, X-VFS-Peer-Id"}
    });

    svr->Options(R"(.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
    });

    svr->Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"status", "OK"}, {"id", config_.id}}).dump(), "application/json");
    });

    svr->Post("/register", [this](const httplib::Request& req, httplib::Response& res) {
        std::string peerId;
        std::string url;

        // 1. Try parsing JSON body
        if (!req.body.empty()) {
            try {
                json body = json::parse(req.body);
                if (body.contains("id")) peerId = body["id"];
                else if (body.contains("peerId")) peerId = body["peerId"];
                if (body.contains("url")) url = body["url"];
            } catch (...) {}
        }

        // 2. Try query parameters if JSON failed
        if (peerId.empty() || url.empty()) {
            auto get_param = [&](const std::string& key) {
                if (req.has_param(key)) return req.get_param_value(key);
                size_t pos = req.target.find("?");
                if (pos != std::string::npos) {
                    std::string query = req.target.substr(pos + 1);
                    size_t kpos = query.find(key + "=");
                    if (kpos != std::string::npos) {
                        size_t vpos = kpos + key.length() + 1;
                        size_t vend = query.find("&", vpos);
                        std::string val = query.substr(vpos, vend != std::string::npos ? vend - vpos : std::string::npos);
                        return val;
                    }
                }
                return std::string("");
            };
            if (peerId.empty()) {
                peerId = get_param("peerId");
                if (peerId.empty()) peerId = get_param("id");
            }
            if (url.empty()) url = get_param("url");
        }

        if (!peerId.empty()) {
            if (!url.empty()) {
                add_peer(url);
                res.set_content(json({{"id", config_.id}, {"reachability", "DIRECT"}}).dump(), "application/json");
            } else {
                {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    if (!peers_.count(peerId)) {
                        auto conn = std::make_shared<ReverseConnection>(peerId);
                        peers_[peerId] = conn;
                        std::cout << "[VFSNode " << config_.id << "] Reverse Peer Added: " << peerId << std::endl;

                        // Forward existing interests to the new peer
                        {
                            std::lock_guard<std::mutex> ilock(interest_mutex_);
                            long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                            for (auto const& [sel_str, subs] : interests_) {
                                long long maxExp = 0;
                                for (auto const& [neighbor_id, expiry] : subs) if (expiry > maxExp) maxExp = expiry;
                                if (maxExp > now) {
                                    conn->subscribe(interest_selectors_[sel_str], maxExp, {config_.id});
                                }
                            }
                        }
                    }
                }
                res.set_content(json({{"id", config_.id}, {"reachability", "REVERSE"}}).dump(), "application/json");
            }
        } else {
            res.status = 400;
            res.set_content("Missing peerId", "text/plain");
        }
    });

    svr->Post("/read", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            VFSRequest vreq;
            if (body.contains("cid") && body["cid"].is_string()) {
                vreq.cid = body["cid"].get<std::string>();
            } else if (body.contains("selector")) {
                vreq.selector = Selector::from_json(body["selector"]);
                vreq.selector.validate(); // CRITICAL: FAIL VISIBLY
            } else {
                throw VFSException("Missing cid or selector in read request", 400);
            }

            vreq.stack = body.value("stack", std::vector<std::string>{});
            vreq.resolutionStack = body.value("resolutionStack", std::vector<std::string>{});
            if (vreq.stack.empty() && req.has_header("x-vfs-id")) {
                vreq.stack.push_back(req.get_header_value("x-vfs-id"));
            }
            vreq.expiresAt = body.value("expiresAt", 0LL);
            vreq.followLinks = body.value("followLinks", true);
            
            // Critical: If it's a selector, compute CID before read_impl to ensure 
            // we use the canonical key for local lookup.
            std::string cid = vreq.is_cid() ? vreq.cid : get_cid(vreq.selector);

            auto data = read_impl(vreq);
            
            // Try to find metadata to set type header
            json info = {{"state", "AVAILABLE"}};
            {
                std::lock_guard<std::mutex> lock(storage_mutex_);
                std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
                if (std::filesystem::exists(mp)) {
                    std::ifstream in(mp);
                    json meta;
                    try { in >> meta; info = meta; } catch(...) {}
                }
            }
            res.set_header("x-vfs-info", encode_safe(info));
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
        } catch (const VFSException& e) {
            res.status = e.code;
            res.set_content(e.what(), "text/plain");
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/subscribe", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            // Protocol Violation Check: This will throw if selector is malformed
            Selector s = Selector::from_json(body.at("selector"));
            
            std::cout << "[REST " << config_.id << "] POST /subscribe " << s.path << std::endl;
            subscribe(s.to_json(), body.at("expiresAt"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            std::cerr << "[REST " << config_.id << "] /subscribe Error: " << e.what() << std::endl;
            res.status = 500;
            res.set_content(e.what(), "text/plain");
        }
    });

    svr->Post("/notify", [this](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            json selector_json = body.at("selector");
            if (!selector_json.is_null()) {
                // Protocol Violation Check: This will throw if selector is malformed
                Selector::from_json(selector_json);
            }
            notify(selector_json, body.at("payload"), body.value("stack", std::vector<std::string>{}));
            res.status = 200;
        } catch (const std::exception& e) {
            res.status = 500;
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
        // std::cout << "[REST " << config_.id << "] /listen from " << peer_id << std::endl;
        register_reverse_peer(peer_id, res);
    });

    svr->listen("0.0.0.0", config_.port);
}

void VFSNode::stop() {
    if (server_ptr_) {
        auto svr = (httplib::Server*)server_ptr_;
        svr->stop();
        delete svr;
        server_ptr_ = nullptr;
    }
}

std::vector<uint8_t> VFSNode::read_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) {
            throw VFSException("Request expired", 404);
        }
    }

    std::string target_cid;
    if (req.is_cid()) {
        target_cid = req.cid;
    } else {
        target_cid = get_cid(req.selector);
    }

    if (has_local(target_cid)) {
        // Check if it's a LINK in .meta
        bool isLink = false;
        Selector linkTarget;

        if (req.followLinks) {
            std::lock_guard<std::mutex> lock(storage_mutex_);
            std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (target_cid + ".meta");
            if (std::filesystem::exists(mp)) {
                std::ifstream in(mp);
                json meta;
                try {
                    in >> meta;
                    if (meta.value("state", "") == "LINK" && meta.contains("target")) {
                        isLink = true;
                        linkTarget = Selector::from_json(meta["target"]);
                    }
                } catch (...) {}
            }
        }

        if (isLink) {
            // Cycle Protection: Ensure this CID isn't already in the resolution stack
            if (std::find(req.resolutionStack.begin(), req.resolutionStack.end(), target_cid) != req.resolutionStack.end()) {
                throw VFSException("Infinite Link Cycle detected for CID: " + target_cid, 500);
            }

            VFSRequest linkReq = req;
            linkReq.cid = "";
            linkReq.selector = linkTarget;
            linkReq.resolutionStack.push_back(target_cid);
            try {
                return read_impl(linkReq);
            } catch (...) {
                // If link target not found locally, continue to mesh discovery
            }
        }

        // ONLY return local if .data actually exists
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (target_cid + ".data");
        if (std::filesystem::exists(dp)) {
            return get_local(target_cid);
        }
    }

    // --- Computational Fulfillment (Handlers) ---
    // Only triggered if request is a Selector
    if (!req.is_cid()) {
        OpHandler handler;
        {
            std::lock_guard<std::mutex> lock(handlers_mutex_);
            if (handlers_.count(req.selector.path)) {
                handler = handlers_[req.selector.path];
            }
        }

        if (handler) {
            handler(req);
            // After handler executes, the data MUST be in local storage
            std::string target_cid = get_cid(req.selector);
            if (has_local(target_cid)) {
                return get_local(target_cid);
            }
            std::cerr << "[VFS] Fulfillment Failure for path: " << req.selector.path << " output: " << req.selector.output << std::endl;
            std::cerr << "      Expected CID: " << target_cid << std::endl;
            std::cerr << "      Selector JSON: " << req.selector.to_json().dump() << std::endl;
            throw VFSException("Handler failed to fulfill identity: " + target_cid + " for path: " + req.selector.path);
        }
    }

    // --- Mesh Discovery ---
    {
        for (const auto& neighbor : config_.neighbors) {
            // Protocol Rule: Don't dispatch to neighbors already in the stack
            // We use a simple string match on the neighbor URL for now, 
            // but the neighbor will also check the stack themselves.
            bool already_in_stack = false;
            for (const auto& s : req.stack) {
                if (neighbor.find(s) != std::string::npos) already_in_stack = true;
            }
            if (already_in_stack) continue;

            VFSRequest forwardReq = req;
            forwardReq.stack.push_back(config_.id);

            httplib::Client cli(neighbor);
            httplib::Headers headers = { {"x-vfs-id", config_.id} };
            
            json body = {{"stack", forwardReq.stack}, {"expiresAt", forwardReq.expiresAt}, {"followLinks", forwardReq.followLinks}};
            if (forwardReq.is_cid()) {
                body["cid"] = forwardReq.cid;
            } else {
                body["selector"] = forwardReq.selector;
            }

            auto res = cli.Post("/read", headers, body.dump(), "application/json");
            if (res && res->status == 200) {
                std::vector<uint8_t> data(res->body.begin(), res->body.end());
                return data;
            }
        }
    }

    // --- Final Fallback ---
    throw VFSException("Content not found for " + (req.is_cid() ? ("CID: " + req.cid) : ("Selector: " + req.selector.path)), 404);
}

std::string VFSNode::get_cid(const Selector& sel) {
    // Protocol Rule: Hash the canonical binary representation (JCB) directly.
    return vfs_hash256(encode_jcb(sel.to_json()));
}

bool VFSNode::has_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
    return std::filesystem::exists(p) || std::filesystem::exists(mp);
}

std::vector<uint8_t> VFSNode::get_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::ifstream in(p, std::ios::binary);
    if (!in) return {};
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data) {
    std::string cid = get_cid(sel);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());

        json meta = {
            {"state", "AVAILABLE"},
            {"type", "json"},
            {"selector", sel.to_json()}
        };
        std::ofstream mos(mp);
        mos << meta.dump();
    }

    notify(sel.to_json(), {{"state", "AVAILABLE"}});
    return sel;
}

template<> CID VFSNode::materialize<std::vector<uint8_t>>(const std::vector<uint8_t>& data) {
    // For terminal mathematical artifacts (raw bytes), always use direct binary hash.
    // This ensures consistency with the Processor and global identity rules.
    std::string cid_str = vfs_hash256(data);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid_str + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid_str + ".meta");
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        if (!data.empty()) os.write((const char*)data.data(), data.size());
        else os << ""; // Ensure file exists for empty data
        
        json meta = {{"state", "AVAILABLE"}, {"type", "bytes"}};
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    
    return CID{cid_str};
}

void VFSNode::ForwardConnection::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    json body = {{"selector", selector}, {"payload", payload}, {"stack", stack}, {"type", "NOTIFY"}};
    std::string target_url = url;
    std::thread([target_url, body]() {
        httplib::Client cli(target_url);
        cli.Post("/notify", body.dump(), "application/json");
    }).detach();
}

void VFSNode::ForwardConnection::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    json body = {{"selector", selector}, {"expiresAt", expiresAt}, {"stack", stack}};
    std::string target_url = url;
    std::thread([target_url, body]() {
        httplib::Client cli(target_url);
        cli.Post("/subscribe", body.dump(), "application/json");
    }).detach();
}

std::vector<uint8_t> VFSNode::ForwardConnection::read(const VFSRequest& req) {
    httplib::Client cli(url);
    json body = {{"stack", req.stack}, {"resolutionStack", req.resolutionStack}, {"expiresAt", req.expiresAt}};
    if (req.is_cid()) body["cid"] = req.cid;
    else body["selector"] = req.selector;
    
    if (auto res = cli.Post("/read", body.dump(), "application/json")) {
        if (res->status == 200) {
            return std::vector<uint8_t>(res->body.begin(), res->body.end());
        }
    }
    throw VFSException("Forward read failed", 500);
}

void VFSNode::ReverseConnection::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_back({{"type", "NOTIFY"}, {"selector", selector}, {"payload", payload}, {"stack", stack}});
    cv.notify_one();
}

void VFSNode::ReverseConnection::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_back({{"type", "COMMAND"}, {"op", "SUB"}, {"selector", selector}, {"expiresAt", expiresAt}, {"stack", stack}});
    cv.notify_one();
}

std::vector<uint8_t> VFSNode::ReverseConnection::read(const VFSRequest& req) {
    // Note: C++ reverse read requires a registry to track replies.
    // For now, we only support reverse notify.
    throw VFSException("C++ Reverse READ not yet implemented", 501);
}

void VFSNode::notify(const json& selector_json, const json& payload, const std::vector<std::string>& stack) {
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;
    
    Selector selector = Selector::from_json(selector_json);
    std::string sel_str = encode_safe(selector.to_json());
    
    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        if (interests_.count(sel_str)) {
            auto& subs = interests_[sel_str];
            std::lock_guard<std::mutex> plock(peer_mutex_);
            for (auto const& [peer_id, expiresAt] : subs) {
                if (std::find(stack.begin(), stack.end(), peer_id) != stack.end()) continue;
                if (peers_.count(peer_id)) targets.push_back(peers_[peer_id]);
            }
        }
    }

    std::vector<std::string> next_stack = stack;
    next_stack.push_back(config_.id);
    for (auto& conn : targets) {
        conn->notify(selector.to_json(), payload, next_stack);
    }
}

void VFSNode::subscribe(const json& selector_json, long long expiresAt, const std::vector<std::string>& stack) {
    Selector selector = Selector::from_json(selector_json);
    std::string sel_str = encode_safe(selector.to_json());
    std::string peer_id = stack.empty() ? "unknown" : stack.back();
    
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        interests_[sel_str][peer_id] = expiresAt;
        interest_selectors_[sel_str] = selector.to_json();
    }

    if (selector.path == "sys/schema") {
        std::shared_ptr<Connection> target;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            if (peers_.count(peer_id)) target = peers_[peer_id];
        }

        if (target) {
            json catalog = get_catalog();
            json payload = {{"type", "CATALOG_ANNOUNCEMENT"}, {"catalog", catalog["catalog"]}, {"provider", config_.id}};
            target->notify(selector.to_json(), payload, {config_.id});
        }
    }

    // PROPAGATION: Forward interest to all OTHER peers
    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        std::vector<std::string> next_stack = stack;
        next_stack.push_back(config_.id);
        
        for (auto const& [id, conn] : peers_) {
            // Don't send back to any node already in the stack
            bool in_stack = false;
            for (const auto& s : stack) if (s == id) { in_stack = true; break; }
            if (in_stack) continue;

            targets.push_back(conn);
        }
    }

    for (auto& conn : targets) {
        conn->subscribe(selector.to_json(), expiresAt, stack);
    }
}

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {
    std::shared_ptr<ReverseConnection> conn;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (peers_.count(peer_id)) {
            conn = std::dynamic_pointer_cast<ReverseConnection>(peers_[peer_id]);
        }
    }

    if (!conn) {
        res.status = 404;
        return;
    }

    std::unique_lock<std::mutex> lock(conn->mutex);
    
    // Zombie Trap: Reject if already polling
    if (conn->is_polling) {
        res.status = 409;
        res.set_content("CRITICAL MESH VIOLATION: Duplicate /listen received for peer. Previous poll is still active.", "text/plain");
        return;
    }

    conn->is_polling = true;

    if (conn->queue.empty()) {
        conn->cv.wait_for(lock, std::chrono::seconds(30), [&conn] {
            return !conn->queue.empty();
        });
    }

    conn->is_polling = false;

    if (!conn->queue.empty()) {
        json cmd = conn->queue.front();
        conn->queue.erase(conn->queue.begin());
        res.set_content(cmd.dump(), "application/json");
        return;
    }
    res.status = 204;
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
    for (auto const& [id, conn] : peers_) {
        std::string url = "";
        if (!conn->is_reverse()) {
            url = std::static_pointer_cast<ForwardConnection>(conn)->url;
        }
        neighbors.push_back({{"id", id}, {"url", url}, {"reachability", conn->is_reverse() ? "REVERSE" : "DIRECT"}});
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
                std::shared_ptr<ForwardConnection> conn;
                {
                    std::lock_guard<std::mutex> lock(peer_mutex_);
                    conn = std::make_shared<ForwardConnection>(id, url);
                    peers_[id] = conn;
                }
                std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << " (" << url << ")" << std::endl;
                
                // 1. Forward existing interests to the new peer
                {
                    std::lock_guard<std::mutex> lock(interest_mutex_);
                    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    for (auto const& [sel_str, subs] : interests_) {
                        long long maxExp = 0;
                        for (auto const& [neighbor_id, expiry] : subs) if (expiry > maxExp) maxExp = expiry;
                        if (maxExp > now) {
                            conn->subscribe(interest_selectors_[sel_str], maxExp, {config_.id});
                        }
                    }
                }

                // 2. Announce Catalog to the new peer
                json catalog = get_catalog();
                conn->notify({{"path", "sys/schema"}}, {{"type", "CATALOG_ANNOUNCEMENT"}, {"provider", config_.id}, {"catalog", catalog["catalog"]}}, {});

                // 3. Update Topology
                notify({{"path", "sys/topo"}}, {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", get_neighbors()}}, {});
            } catch (...) {}
        }
    }
}

void VFSNode::link(const Selector& src, const Selector& tgt) {
    // Basic link logic: create a meta file for src that points to tgt
    std::string srcKey = get_cid(src);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (srcKey + ".meta");
    json meta = {{"selector", src.to_json()}, {"state", "LINK"}, {"target", tgt.to_json()}};
    std::ofstream os(mp);
    os << meta.dump();
}

} // namespace fs
