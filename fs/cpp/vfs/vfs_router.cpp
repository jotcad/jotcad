#include "../vfs_node.h"
#include "../cid.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../vendor/httplib.h"
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

        std::string last_404_msg = "";
        for (const auto& conn : targets) {
            VFSRequest forwardReq = req;
            forwardReq.stack.push_back(config_.id);
            try {
                return conn->read(forwardReq);
            } catch (const VFSException& e) {
                if (e.code == 404) {
                    last_404_msg = e.what();
                    continue;
                }
                // Prepend local context for non-404 errors
                json ctx = {{"vfsId", config_.id}, {"target", conn->neighbor_id}, {"path", req.selector.path}};
                std::string newMsg = ctx.dump() + "\n" + e.what();
                throw VFSException(newMsg, e.code);
            } catch (const std::exception& e) {
                std::cerr << "[VFSNode " << config_.id << "] Peer Exception: " << e.what() << std::endl;
            }
        }
        
        throw VFSException(last_404_msg.empty() ? ("Content not found for " + (req.is_cid() ? ("CID: " + req.cid) : ("Selector: " + req.selector.path))) : last_404_msg, 404);
    }
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

    if (selector.path == "sys/topo") {
        std::shared_ptr<Connection> target;
        json neighbors = json::array();
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            if (peers_.count(peer_id)) target = peers_[peer_id];
            for (auto const& [p_id, conn] : peers_) {
                neighbors.push_back(json{{"id", p_id}, {"reachability", "REVERSE"}});
            }
        }

        if (target) {
            json payload = {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", neighbors}};
            std::cout << "[VFSNode " << config_.id << "] -> Replying to sys/topo sub from " << peer_id << " with local topology" << std::endl;
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
