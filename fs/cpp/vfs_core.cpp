#include "vfs_node.h"
#include "cid.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
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

void VFSNode::add_connection(std::shared_ptr<Connection> conn) {
    if (!conn) return;
    std::string id = conn->neighbor_id;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peers_[id] = conn;
    }
    std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << (conn->is_reverse() ? " (REVERSE)" : " (DIRECT)") << std::endl;
    
    // Protocol Rule: Only sync interests on connection. 
    // Announcements (Catalog/Topo) happen on-demand via subscriptions.
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
        try {
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
                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(peer_mutex_);
                        if (peers_.count(peerId)) exists = true;
                    }
                    if (!exists) {
                        add_connection(std::make_shared<ReverseConnection>(peerId));
                    }
                    res.set_content(json({{"id", config_.id}, {"reachability", "REVERSE"}}).dump(), "application/json");
                }
            } else {
                res.status = 400;
                res.set_content("Missing peerId", "text/plain");
            }
        } catch (const std::exception& e) {
            std::cerr << "[VFSNode " << config_.id << "] /register CRITICAL ERROR: " << e.what() << std::endl;
            res.status = 500;
            res.set_content(std::string("Internal Server Error: ") + e.what(), "text/plain");
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
            
            auto result = read_impl(vreq);
            
            // Base64 encode the metadata directly
            std::string info_str = result.metadata.dump();
            std::vector<uint8_t> info_bytes(info_str.begin(), info_str.end());
            std::string info_b64 = fs::base64_encode(info_bytes);

            res.set_header("x-vfs-info", info_b64);
            res.set_content((const char*)result.data.data(), result.data.size(), "application/octet-stream");
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

    std::cout << "[VFSNode " << config_.id << "] Internal server listening on 0.0.0.0:" << config_.port << "..." << std::endl;
    if (!svr->listen("0.0.0.0", config_.port)) {
        std::cerr << "[VFSNode " << config_.id << "] CRITICAL: Failed to bind to 0.0.0.0:" << config_.port << ". The port may be in use." << std::endl;
    }
    std::cout << "[VFSNode " << config_.id << "] Server has stopped." << std::endl;
}

void VFSNode::stop() {
    if (server_ptr_) {
        auto svr = (httplib::Server*)server_ptr_;
        svr->stop();
        delete svr;
        server_ptr_ = nullptr;
    }
}

VFSResult VFSNode::read_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) {
            throw VFSException("Request expired", 404);
        }
    }

    std::string target_cid = req.is_cid() ? req.cid : get_cid(req.selector);

    std::cout << "[VFSNode " << config_.id << "] Resolving: " << (req.is_cid() ? req.cid : req.selector.to_json().dump()) << " (stack: ";
    for(size_t i=0; i<req.stack.size(); ++i) std::cout << (i==0?"":",") << req.stack[i];
    std::cout << ")" << std::endl;

    if (has_local(target_cid)) {
        auto res = get_local(target_cid);
        
        // Protocol Rule: Formal Link Following (encoding: "link")
        if (req.followLinks && res.metadata.value("encoding", "") == "link") {
            // Cycle Protection
            if (std::find(req.resolutionStack.begin(), req.resolutionStack.end(), target_cid) != req.resolutionStack.end()) {
                throw VFSException("Infinite Link Cycle detected for CID: " + target_cid, 500);
            }

            try {
                json link_json = json::parse(res.data);
                VFSRequest linkReq = req;
                linkReq.cid = "";
                linkReq.selector = Selector::from_json(link_json);
                linkReq.resolutionStack.push_back(target_cid);
                return read_impl(linkReq);
            } catch (...) {
                // If link resolution fails, continue to handlers/mesh
            }
        }

        // Return local if data is available
        if (!res.data.empty() || res.metadata.value("state", "") == "AVAILABLE") {
            return res;
        }
    }

    // --- Computational Fulfillment (Handlers) ---
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
            std::string fulfilled_cid = get_cid(req.selector);
            if (has_local(fulfilled_cid)) {
                return get_local(fulfilled_cid);
            }
            throw VFSException("Handler failed to fulfill identity: " + fulfilled_cid);
        }
    }

    // --- Mesh Discovery ---
    {
        std::vector<std::shared_ptr<Connection>> targets;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            for (auto const& [id, conn] : peers_) {
                if (std::find(req.stack.begin(), req.stack.end(), id) != req.stack.end()) continue;
                targets.push_back(conn);
            }
        }

        for (const auto& conn : targets) {
            VFSRequest forwardReq = req;
            forwardReq.stack.push_back(config_.id);
            try {
                return conn->read(forwardReq);
            } catch (...) {}
        }
    }

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

VFSResult VFSNode::get_local(const std::string& cid) {
    VFSResult res;
    res.metadata = {{"state", "PENDING"}, {"cid", cid}};

    std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (std::filesystem::exists(mp)) {
        std::ifstream in(mp);
        try { in >> res.metadata; } catch(...) {}
    }

    if (std::filesystem::exists(dp)) {
        std::ifstream in(dp, std::ios::binary);
        res.data = std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        res.metadata["state"] = "AVAILABLE";
    }

    return res;
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
            {"encoding", "bytes"}, // Raw bytes via Selector
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
        
        json meta = {{"state", "AVAILABLE"}, {"encoding", "bytes"}};
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
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli.enable_server_certificate_verification(false);
#endif
        cli.Post("/notify", body.dump(), "application/json");
    }).detach();
}

void VFSNode::ForwardConnection::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    json body = {{"selector", selector}, {"expiresAt", expiresAt}, {"stack", stack}};
    std::string target_url = url;
    std::thread([target_url, body]() {
        httplib::Client cli(target_url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli.enable_server_certificate_verification(false);
#endif
        cli.Post("/subscribe", body.dump(), "application/json");
    }).detach();
}

VFSResult VFSNode::ForwardConnection::read(const VFSRequest& req) {
    httplib::Client cli(url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    cli.enable_server_certificate_verification(false);
#endif
    json body = {{"stack", req.stack}, {"resolutionStack", req.resolutionStack}, {"expiresAt", req.expiresAt}};
    if (req.is_cid()) body["cid"] = req.cid;
    else body["selector"] = req.selector;
    
    if (auto res = cli.Post("/read", body.dump(), "application/json")) {
        if (res->status == 200) {
            VFSResult result;
            result.data = std::vector<uint8_t>(res->body.begin(), res->body.end());
            
            if (res->has_header("x-vfs-info")) {
                std::string info_b64 = res->get_header_value("x-vfs-info");
                std::vector<uint8_t> info_bytes = fs::base64_decode(info_b64);
                std::string info_str(info_bytes.begin(), info_bytes.end());
                try {
                    result.metadata = json::parse(info_str);
                } catch(...) {
                    result.metadata = {{"state", "AVAILABLE"}};
                }
            } else {
                result.metadata = {{"state", "AVAILABLE"}};
            }
            return result;
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

VFSResult VFSNode::ReverseConnection::read(const VFSRequest& req) {
    // Note: C++ reverse read requires a registry to track replies.
    // For now, we only support reverse notify.
    throw VFSException("C++ Reverse READ not yet implemented", 501);
}

void VFSNode::notify(const json& selector_json, const json& payload, const std::vector<std::string>& stack) {
    // 1. Stack Protection: Break loops early
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;

    Selector selector;
    if (!selector_json.is_null()) {
        selector = Selector::from_json(selector_json);
    }

    std::string sel_str = encode_safe(selector.to_json());
    
    std::cout << "[VFSNode " << config_.id << "] notify: " << (selector.path.empty() ? "null" : selector.path) << " from stack {";
    for(size_t i=0; i<stack.size(); ++i) std::cout << (i==0?"":",") << stack[i];
    std::cout << "}" << std::endl;

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
        std::cout << "[VFSNode " << config_.id << "] -> Forwarding notification for " << selector.path << " to " << conn->neighbor_id << std::endl;
        conn->notify(selector.to_json(), payload, next_stack);
    }
}

void VFSNode::subscribe(const json& selector_json, long long expiresAt, const std::vector<std::string>& stack) {
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;

    Selector selector = Selector::from_json(selector_json);
    std::string sel_str = encode_safe(selector.to_json());
    std::string peer_id = stack.empty() ? "unknown" : stack.back();

    std::cout << "[VFSNode " << config_.id << "] subscribe: " << selector.path << " from " << peer_id << " (stack size: " << stack.size() << ")" << std::endl;
    
    bool isNewInterest = false;
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        if (interests_[sel_str].count(peer_id) == 0) isNewInterest = true;
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
            std::cout << "[VFSNode " << config_.id << "] -> Replying to sys/schema sub from " << peer_id << " with local catalog (" << catalog["catalog"].size() << " ops)" << std::endl;
            target->notify(selector.to_json(), payload, {config_.id});
        }
    }

    // PROPAGATION: Forward interest to all OTHER peers
    if (isNewInterest) {
        std::vector<std::shared_ptr<Connection>> others;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            for (auto const& [id, conn] : peers_) {
                // Don't send back to any node already in the stack
                bool in_stack = false;
                for (const auto& s : stack) if (s == id) { in_stack = true; break; }
                if (in_stack) continue;

                others.push_back(conn);
            }
        }

        std::vector<std::string> next_stack = stack;
        next_stack.push_back(config_.id);
        for (auto& conn : others) {
            std::cout << "[VFSNode " << config_.id << "] -> Propagating interest in " << selector.path << " from " << peer_id << " to " << conn->neighbor_id << std::endl;
            conn->subscribe(selector.to_json(), expiresAt, next_stack);
        }
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
        neighbors.push_back({{"id", id}, {"url", conn->get_url()}, {"reachability", conn->is_reverse() ? "REVERSE" : "DIRECT"}});
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
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    cli.enable_server_certificate_verification(false);
#endif
    if (auto res = cli.Get("/health")) {
        if (res->status == 200) {
            try {
                json body = json::parse(res->body);
                std::string id = body["id"];
                add_connection(std::make_shared<ForwardConnection>(id, url));
            } catch (...) {}
        }
    }
}

void VFSNode::link(const Selector& src, const Selector& tgt) {
    // Protocol Rule: A Link is an artifact whose content is another address (a Selector).
    // Metadata (.meta): MUST contain state: "AVAILABLE" and encoding: "link".
    // Data (.data): MUST contain the Target Selector serialized as JSON.
    std::string srcKey = get_cid(src);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (srcKey + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (srcKey + ".meta");
    
    {
        std::ofstream os(dp);
        os << tgt.to_json().dump();
    }

    json meta = {
        {"selector", src.to_json()},
        {"state", "AVAILABLE"},
        {"encoding", "link"}
    };
    std::ofstream os(mp);
    os << meta.dump();
}

} // namespace fs
