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

VFSResult VFSNode::read_cid_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) throw VFSException("Request expired", 404);
    }

    std::cout << "[VFSNode " << config_.id << "] Resolving CID: " << req.cid << " (stack: ";
    for(size_t i=0; i<req.stack.size(); ++i) std::cout << (i==0?"":",") << req.stack[i];
    std::cout << ")" << std::endl;

    bool is_backflow = std::find(req.stack.begin(), req.stack.end(), config_.id) != req.stack.end();

    if (has_local(req.cid)) {
        auto res = get_local(req.cid);
        if (!res.data.empty() || res.metadata.value("state", "") == "AVAILABLE") return res;
    }

    if (is_backflow) throw VFSException("Backflow detected", 403);

    // --- Mesh Discovery ---
    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [id, conn] : peers_) targets.push_back(conn);
    }

    std::string last_404_msg = "";
    for (const auto& conn : targets) {
        VFSRequest forwardReq = req;
        forwardReq.stack.push_back(config_.id);
        forwardReq.op = "READ_CID";
        try {
            return conn->sendRequest(forwardReq);
        } catch (const VFSException& e) {
            if (e.code == 404 || e.code == 403) {
                if (last_404_msg.empty() || e.code == 404) last_404_msg = e.what();
                continue;
            }
            json ctx = {{"vfsId", config_.id}, {"target", conn->neighbor_id}, {"cid", req.cid}};
            throw VFSException(ctx.dump() + "\n" + e.what(), e.code);
        } catch (...) { std::cerr << "Peer Exception" << std::endl; }
    }
    throw VFSException(last_404_msg.empty() ? ("Content not found for CID: " + req.cid) : last_404_msg, 404);
}

VFSResult VFSNode::read_selector_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) throw VFSException("Request expired", 404);
    }

    std::string target_cid = get_cid(req.selector);

    std::cout << "[VFSNode " << config_.id << "] Resolving Selector: " << req.selector.to_json().dump() << " (stack: ";
    for(size_t i=0; i<req.stack.size(); ++i) std::cout << (i==0?"":",") << req.stack[i];
    std::cout << ")" << std::endl;

    bool is_backflow = std::find(req.stack.begin(), req.stack.end(), config_.id) != req.stack.end();

    if (has_local(target_cid)) {
        auto res = get_local(target_cid);
        
        if (req.followLinks && res.metadata.value("encoding", "") == "link") {
            if (std::find(req.resolutionStack.begin(), req.resolutionStack.end(), target_cid) != req.resolutionStack.end()) {
                throw VFSException("Infinite Link Cycle detected for CID: " + target_cid, 500);
            }
            try {
                json link_json = json::parse(res.data);
                VFSRequest linkReq = req;
                linkReq.op = "READ_SELECTOR";
                linkReq.cid = "";
                linkReq.selector = Selector::from_json(link_json);
                linkReq.resolutionStack.push_back(target_cid);
                return read_selector_impl(linkReq);
            } catch (...) {}
        }

        if (!res.data.empty() || res.metadata.value("state", "") == "AVAILABLE") {
            return res;
        }
    }

    OpHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.count(req.selector.path)) handler = handlers_[req.selector.path];
    }
    if (handler) {
        handler(req);
        std::string fulfilled_cid = get_cid(req.selector);
        if (has_local(fulfilled_cid)) return get_local(fulfilled_cid);
        throw VFSException("Handler failed to fulfill identity: " + fulfilled_cid);
    }

    if (is_backflow) throw VFSException("Backflow detected", 403);

    // --- Mesh Discovery ---
    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [id, conn] : peers_) targets.push_back(conn);
    }

    std::string last_404_msg = "";
    for (const auto& conn : targets) {
        VFSRequest forwardReq = req;
        forwardReq.stack.push_back(config_.id);
        forwardReq.op = "READ_SELECTOR";
        try {
            return conn->sendRequest(forwardReq);
        } catch (const VFSException& e) {
            if (e.code == 404 || e.code == 403) {
                if (last_404_msg.empty() || e.code == 404) last_404_msg = e.what();
                continue;
            }
            json ctx = {{"vfsId", config_.id}, {"target", conn->neighbor_id}, {"path", req.selector.path}};
            throw VFSException(ctx.dump() + "\n" + e.what(), e.code);
        } catch (...) { std::cerr << "Peer Exception" << std::endl; }
    }
    
    throw VFSException(last_404_msg.empty() ? ("Content not found for Selector: " + req.selector.path) : last_404_msg, 404);
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

void VFSNode::notify(const Selector& selector, const json& payload, const std::vector<std::string>& stack) {
    // 1. Stack Protection: Break loops early
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;

    std::cout << "[VFSNode " << config_.id << "] notify: " << (selector.path.empty() ? "null" : selector.path) << " from stack {";
    for(size_t i=0; i<stack.size(); ++i) std::cout << (i==0?"":",") << stack[i];
    std::cout << "}" << std::endl;

    std::vector<std::shared_ptr<Connection>> targets;
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        
        for (auto& entry : interests_) {
            if (entry.selector == selector) {
                std::lock_guard<std::mutex> plock(peer_mutex_);
                for (auto const& [peer_id, expiresAt] : entry.subs) {
                    if (expiresAt > now && std::find(stack.begin(), stack.end(), peer_id) == stack.end()) {
                        if (peers_.count(peer_id)) targets.push_back(peers_[peer_id]);
                    }
                }
            }
        }
    }

    std::vector<std::string> next_stack = stack;
    next_stack.push_back(config_.id);
    for (auto& conn : targets) {
        std::cout << "[VFSNode " << config_.id << "] -> Forwarding notification for " << selector.path << " to " << conn->neighbor_id << std::endl;
        VFSRequest req;
        req.op = "PUB";
        req.selector = selector;
        req.data = payload.dump();
        req.stack = next_stack;
        conn->sendRequest(req);
    }
}

void VFSNode::subscribe(const Selector& selector, long long expiresAt, const std::vector<std::string>& stack) {
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) {
        std::cout << "[VFSNode " << config_.id << "] DROP subscribe: Loop detected (stack contains " << config_.id << ")" << std::endl;
        return;
    }

    std::string peer_id = stack.empty() ? "unknown" : stack.back();

    std::cout << "[VFSNode " << config_.id << "] subscribe: " << selector.path << " from " << peer_id << " (stack size: " << stack.size() << ")" << std::endl;
    
    bool isNewInterest = false;
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        auto it = std::find_if(interests_.begin(), interests_.end(), [&](const SubscriptionEntry& e) {
            return e.selector == selector;
        });

        if (it == interests_.end()) {
            interests_.push_back({selector, {{peer_id, expiresAt}}, 0});
            isNewInterest = true;
        } else {
            if (it->subs.count(peer_id) == 0) isNewInterest = true;
            it->subs[peer_id] = expiresAt;
        }
    }

    if (isNewInterest) {
        if (selector.path == "sys/topo") {
            std::cout << "[VFSNode " << config_.id << "] -> Replying to sys/topo sub from " << peer_id << " with local topology" << std::endl;
            VFSRequest req;
            req.op = "PUB";
            req.selector = selector;
            req.data = get_topology_payload().dump();
            req.stack = {config_.id};
            
            std::lock_guard<std::mutex> plock(peer_mutex_);
            if (peers_.count(peer_id)) {
                peers_[peer_id]->sendRequest(req);
            }
        } else {
            notify(Selector("sys/topo"), get_topology_payload(), {});
        }
    }

    if (selector.path == "sys/schema") {
        std::shared_ptr<Connection> target;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            if (peers_.count(peer_id)) target = peers_[peer_id];
        }

        if (target) {
            json catalog = get_catalog();
            json payload = {{"catalog", catalog["catalog"]}, {"provider", config_.id}};
            std::cout << "[VFSNode " << config_.id << "] -> Replying to sys/schema sub from " << peer_id << " with local catalog (" << catalog["catalog"].size() << " ops)" << std::endl;
            VFSRequest req;
            req.op = "PUB";
            req.selector = selector;
            req.data = payload.dump();
            req.stack = {config_.id};
            target->sendRequest(req);
        }
    }

    if (selector.path == "sys/topo") {
        std::shared_ptr<Connection> target;
        {
            std::lock_guard<std::mutex> lock(peer_mutex_);
            if (peers_.count(peer_id)) target = peers_[peer_id];
        }

        if (target) {
            std::cout << "[VFSNode " << config_.id << "] -> Replying to sys/topo sub from " << peer_id << " with local topology" << std::endl;
            VFSRequest req;
            req.op = "PUB";
            req.selector = selector;
            req.data = get_topology_payload().dump();
            req.stack = {config_.id};
            target->sendRequest(req);
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
            VFSRequest req;
            req.op = "SUB";
            req.selector = selector;
            req.expiresAt = expiresAt;
            req.stack = next_stack;
            conn->sendRequest(req);
        }
    }
}

json VFSNode::get_catalog() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    json catalog = json::object();
    for (auto const& [path, schema] : schemas_) {
        catalog[path] = schema;
    }
    return {{"catalog", catalog}, {"provider", config_.id}};
}

json VFSNode::get_neighbors() {
    std::lock_guard<std::mutex> lock(peer_mutex_);
    json neighbors = json::array();
    for (auto const& [id, conn] : peers_) {
        neighbors.push_back({{"id", id}, {"url", conn->get_url()}, {"reachability", conn->is_reverse() ? "REVERSE" : "DIRECT"}});
    }
    return neighbors;
}

void VFSNode::handle_ws_frame(const json& frame) {
    std::string type = frame.value("type", "");
    if (type == "READ_RESPONSE" || type == "SPY_RESPONSE") {
        std::string tx_id = frame.value("txId", "");
        int status = frame.value("status", 200);
        if (status == 200) {
            VFSResult result;
            if (frame.contains("payload")) {
                json payload = frame["payload"];
                if (payload.is_string()) {
                    result.data = fs::base64_decode(payload.get<std::string>());
                } else if (payload.contains("data") && payload["data"].is_string()) {
                    result.data = fs::base64_decode(payload["data"].get<std::string>());
                }
                if (payload.contains("metadata")) result.metadata = payload["metadata"];
            }
            if (frame.contains("metadata")) result.metadata = frame["metadata"];
            resolve_transaction(tx_id, result);
        } else {
            std::string error = frame.contains("payload") && frame["payload"].contains("error") 
                ? frame["payload"]["error"].get<std::string>() 
                : "Error";
            reject_transaction(tx_id, status, error);
        }
    } else if (type == "READ_SELECTOR" || type == "READ_CID") {
        std::string tx_id = frame.value("txId", "");
        VFSRequest req;
        req.op = type;
        req.expiresAt = frame.value("expiresAt", 0LL);
        if (frame.contains("stack")) {
            for (auto const& s : frame["stack"]) req.stack.push_back(s.get<std::string>());
        }
        if (type == "READ_SELECTOR") {
            req.selector = Selector::from_json(frame["selector"]);
        } else {
            req.cid = frame.value("cid", "");
            if (frame.contains("resolutionStack")) {
                for (auto const& s : frame["resolutionStack"]) req.resolutionStack.push_back(s.get<std::string>());
            }
        }
        
        std::thread([this, tx_id, req]() {
            try {
                VFSResult result = read<VFSResult>(req);
                std::lock_guard<std::mutex> plock(peer_mutex_);
                std::string requester_id = req.stack.empty() ? "" : req.stack.back();
                if (peers_.count(requester_id)) {
                    auto ws_conn = std::dynamic_pointer_cast<WSConnection>(peers_[requester_id]);
                    if (ws_conn) {
                        ws_conn->send_binary_frame({
                            {"type", "READ_RESPONSE"},
                            {"txId", tx_id},
                            {"status", 200},
                            {"metadata", result.metadata},
                            {"hasBinary", true}
                        }, result.data);
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> plock(peer_mutex_);
                std::string requester_id = req.stack.empty() ? "" : req.stack.back();
                if (peers_.count(requester_id)) {
                    auto ws_conn = std::dynamic_pointer_cast<WSConnection>(peers_[requester_id]);
                    if (ws_conn) {
                        ws_conn->send_frame({
                            {"type", "READ_RESPONSE"},
                            {"txId", tx_id},
                            {"status", 500},
                            {"payload", {{"error", e.what()}}}
                        });
                    }
                }
            }
        }).detach();
    } else if (type == "PUB") {
        Selector sel = Selector::from_json(frame["selector"]);
        json payload = frame.value("payload", json::object());
        std::vector<std::string> stack;
        if (frame.contains("stack")) {
            for (auto const& s : frame["stack"]) stack.push_back(s.get<std::string>());
        }
        notify(sel, payload, stack);
    } else if (type == "SUB") {
        Selector sel = Selector::from_json(frame["selector"]);
        long long exp = frame.value("expiresAt", 0LL);
        std::vector<std::string> stack;
        if (frame.contains("stack")) {
            for (auto const& s : frame["stack"]) stack.push_back(s.get<std::string>());
        }
        subscribe(sel, exp, stack);
    }
}

void VFSNode::handle_binary_frame(const json& header, const std::vector<uint8_t>& data) {
    std::string type = header.value("type", "");
    if (type == "READ_RESPONSE" || type == "SPY_RESPONSE") {
        std::string tx_id = header.value("txId", "");
        VFSResult result;
        result.data = data;
        if (header.contains("metadata")) result.metadata = header["metadata"];
        resolve_transaction(tx_id, result);
    } else if (type == "PUB") {
        Selector sel = Selector::from_json(header["selector"]);
        std::vector<std::string> stack;
        if (header.contains("stack")) {
            for (auto const& s : header["stack"]) stack.push_back(s.get<std::string>());
        }
        // Use raw binary notification
        std::string path = sel.path;
        bool hasInterest = false;
        {
            std::lock_guard<std::mutex> lock(interest_mutex_);
            for (auto const& entry : interests_) {
                if (entry.selector.path == path) {
                    hasInterest = true;
                    break;
                }
            }
        }
        if (hasInterest) {
            // Forwarding logic for binary PUB would go here.
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
