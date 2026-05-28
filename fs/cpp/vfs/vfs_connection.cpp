#include "../vfs_node.h"
#include "../cid.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../vendor/httplib.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <random>
#include <iomanip>

namespace fs {

static std::atomic<int> active_poll_threads{0};

static std::string generate_tx_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    std::string res = "tx_";
    for (int i = 0; i < 8; ++i) res += hex[dis(gen)];
    return res;
}

void VFSNode::add_connection(std::shared_ptr<Connection> conn) {
    if (!conn) return;
    std::string id = conn->neighbor_id;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peers_[id] = conn;
    }
    std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << (conn->is_reverse() ? " (REVERSE)" : " (DIRECT)") << std::endl;
    
    // Protocol Rule: Only sync active interests on connection. 
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

    // Broadcast the updated topology to all subscribed peers in the mesh
    json sys_topo = {{"path", "sys/topo"}, {"parameters", json::object()}};
    json neighbors = json::array();
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [p_id, conn_ptr] : peers_) {
            neighbors.push_back(json{{"id", p_id}, {"reachability", conn_ptr->is_reverse() ? "REVERSE" : "DIRECT"}});
        }
    }
    json topo_payload = {{"peer", config_.id}, {"neighbors", neighbors}};
    notify(sys_topo, topo_payload, {});
}

void VFSNode::upgrade_peer_to_ws(const std::string& peer_id, std::shared_ptr<Connection> ws_conn) {
    if (!ws_conn) return;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peers_[peer_id] = ws_conn;
    }
    std::cout << "[VFSNode " << config_.id << "] Peer " << peer_id << " upgraded to WebSocket." << std::endl;
    
    // Trigger topology update
    add_connection(ws_conn);
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

void VFSNode::register_reverse_peer(const std::string& peer_id, httplib::Response& res) {
    struct PollThreadGuard {
        std::atomic<int>& count;
        std::string peer;
        std::string node;
        PollThreadGuard(std::atomic<int>& c, std::string p, std::string n) : count(c), peer(std::move(p)), node(std::move(n)) {}
        ~PollThreadGuard() {
            --count;
        }
    };

    int current_active = ++active_poll_threads;
    if (current_active >= 8) {
        std::cout << "[VFSNode " << config_.id << "] !!! THREAD EXHAUSTION WARNING !!! "
                  << "Total active/blocked long-polling C++ threads: " << current_active << "/8. "
                  << "No remaining threads in the C++ worker pool!" << std::endl;
    }

    PollThreadGuard guard(active_poll_threads, peer_id, config_.id);

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
    
    // Protocol Rule: Superseding (No more 409)
    // If a poll is already active, signal it to exit so the new one can take over.
    if (conn->is_polling) {
        conn->current_poll_id++; // Change ID to invalidate the old thread
        conn->cv.notify_all();
        
        // Wait for old thread to clear is_polling
        conn->cv.wait_for(lock, std::chrono::seconds(2), [&conn] { return !conn->is_polling; });
    }

    long long my_poll_id = ++conn->current_poll_id;
    conn->is_polling = true;

    if (conn->queue.empty()) {
        // Permanent Tunnel: Wait indefinitely for mesh commands or superseding
        conn->cv.wait(lock, [&conn, my_poll_id] {
            return !conn->queue.empty() || conn->current_poll_id != my_poll_id;
        });
    }

    conn->is_polling = false;
    conn->cv.notify_all(); // Wake up any waiting supersede threads

    // If we were superseded, return 204 so the client can try again on the new socket
    if (conn->current_poll_id != my_poll_id) {
        res.status = 204;
        return;
    }

    if (!conn->queue.empty()) {
        json cmd = conn->queue.front();
        conn->queue.erase(conn->queue.begin());
        std::string type = cmd.value("type", "unknown");
        std::string op = cmd.value("op", "none");
        if (op == "none" && type == "PUB") op = "PUB";
        
        std::cout << "[REST " << config_.id << "] -> Deliver " << type << "/" << op << " to " << peer_id << " (Queue: " << conn->queue.size() << ")" << std::endl;
        
        res.set_header("X-VFS-Op", op);
        if (cmd.contains("selector")) res.set_header("X-VFS-Selector", cmd["selector"].dump());
        if (cmd.contains("expiresAt")) res.set_header("X-VFS-Expires", std::to_string(cmd["expiresAt"].get<long long>()));
        
        std::string stack_str;
        if (cmd.contains("stack")) {
            for (size_t i = 0; i < cmd["stack"].size(); ++i) {
                stack_str += (i == 0 ? "" : ",") + cmd["stack"][i].get<std::string>();
            }
        }
        res.set_header("X-VFS-Stack", stack_str);
        
        if (cmd.contains("payload")) {
            if (cmd["payload"].is_binary()) {
                res.set_header("X-VFS-Encoding", "bytes");
                std::vector<uint8_t> data = cmd["payload"].get_binary();
                res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
            } else {
                res.set_header("X-VFS-Encoding", "json");
                res.set_content(cmd["payload"].dump(), "application/json");
            }
        } else {
            res.set_content(cmd.dump(), "application/json");
        }
        return;
    }
    res.status = 204;
}

void VFSNode::ForwardConnection::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    httplib::Headers headers = {
        {"X-VFS-Op", "PUB"},
        {"X-VFS-Selector", selector.dump()},
        {"X-VFS-Stack", ""},
        {"X-VFS-Encoding", "json"},
        {"Content-Type", "application/json"}
    };
    for (size_t i = 0; i < stack.size(); ++i) {
        headers.find("X-VFS-Stack")->second += (i == 0 ? "" : ",") + stack[i];
    }

    std::string body = payload.dump();
    std::string target_url = url;
    std::string neighbor = neighbor_id;

    std::thread([target_url, body, headers, neighbor]() {
        httplib::Client cli(target_url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli.enable_server_certificate_verification(false);
#endif
        if (auto res = cli.Post("/notify", headers, body, "application/json")) {
            if (res->status != 200) {
                std::cerr << "[PROTOCOL ERROR] /notify to " << neighbor << " failed with status " << res->status << ": " << res->body << std::endl;
            }
        } else {
            std::cerr << "[NETWORK ERROR] Failed to reach " << neighbor << " for /notify at " << target_url << std::endl;
        }
    }).detach(); // Still detached, but now it LOGS failures.
}

void VFSNode::ForwardConnection::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    httplib::Headers headers = {
        {"X-VFS-Op", "SUB"},
        {"X-VFS-Selector", selector.dump()},
        {"X-VFS-Expires", std::to_string(expiresAt)},
        {"X-VFS-Stack", ""}
    };
    for (size_t i = 0; i < stack.size(); ++i) {
        headers.find("X-VFS-Stack")->second += (i == 0 ? "" : ",") + stack[i];
    }

    std::string target_url = url;
    std::string neighbor = neighbor_id;
    std::string path = selector.value("path", "unknown");

    std::thread([target_url, headers, neighbor, path]() {
        httplib::Client cli(target_url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        cli.enable_server_certificate_verification(false);
#endif
        if (auto res = cli.Post("/subscribe", headers, "", "application/json")) {
            if (res->status != 200) {
                std::cerr << "[PROTOCOL ERROR] /subscribe (" << path << ") to " << neighbor << " failed with status " << res->status << ": " << res->body << std::endl;
            }
        } else {
            std::cerr << "[NETWORK ERROR] Failed to reach " << neighbor << " for /subscribe at " << target_url << std::endl;
        }
    }).detach();
}

VFSResult VFSNode::ForwardConnection::read(const VFSRequest& req) {
    httplib::Client cli(url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    cli.enable_server_certificate_verification(false);
#endif
    httplib::Headers headers = {
        {"X-VFS-Op", "READ"},
        {"X-VFS-Stack", ""},
        {"X-VFS-Expires", std::to_string(req.expiresAt)}
    };
    for (size_t i = 0; i < req.stack.size(); ++i) {
        headers.find("X-VFS-Stack")->second += (i == 0 ? "" : ",") + req.stack[i];
    }

    std::string body;
    if (req.is_cid()) {
        body = json({{"cid", req.cid}, {"resolutionStack", req.resolutionStack}}).dump();
    } else {
        headers.emplace("X-VFS-Selector", req.selector.to_json().dump());
    }

    if (auto res = cli.Post("/read", headers, body, "application/json")) {
        if (res->status == 200) {
            VFSResult result;
            result.data = std::vector<uint8_t>(res->body.begin(), res->body.end());

            if (res->has_header("x-vfs-info")) {
                std::string info_raw = res->get_header_value("x-vfs-info");
                try {
                    if (!info_raw.empty() && info_raw[0] == '{') {
                        result.metadata = json::parse(info_raw);
                    } else {
                        // Fallback to Base64
                        std::vector<uint8_t> info_bytes = fs::base64_decode(info_raw);
                        result.metadata = json::parse(std::string(info_bytes.begin(), info_bytes.end()));
                    }
                } catch(...) {
                    result.metadata = {{"state", "AVAILABLE"}};
                }
            } else {
                result.metadata = {{"state", "AVAILABLE"}};
            }
            return result;
        } else {
            // JSON Lines error propagation
            json ctx = {{"vfsId", neighbor_id}, {"status", res->status}, {"action", "read_peer"}};
            std::string newBody = ctx.dump() + "\n" + res->body;
            throw VFSException(newBody, res->status);
        }
    }
    throw VFSException(json({{"vfsId", neighbor_id}, {"error", "Network failure"}, {"url", url}}).dump(), 503);
}

void VFSNode::ReverseConnection::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.push_back({{"type", "PUB"}, {"selector", selector}, {"payload", payload}, {"stack", stack}});
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

// --- WebSocket Connection Implementations ---

void VFSNode::WSConnection::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    send_frame({
        {"type", "NOTIFY"},
        {"selector", selector},
        {"payload", payload},
        {"stack", stack}
    });
}

void VFSNode::WSConnection::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    send_frame({
        {"type", "SUBSCRIBE"},
        {"selector", selector},
        {"expiresAt", expiresAt},
        {"stack", stack}
    });
}

VFSResult VFSNode::WSConnection::read(const VFSRequest& req) {
    std::string tx_id = generate_tx_id();
    auto promise = std::make_shared<std::promise<VFSResult>>();
    auto future = promise->get_future();

    {
        std::lock_guard<std::mutex> lock(node->transaction_mutex_);
        node->transactions_[tx_id] = promise;
    }

    json frame = {
        {"type", "READ"},
        {"txId", tx_id},
        {"stack", req.stack},
        {"expiresAt", req.expiresAt}
    };

    if (req.is_cid()) frame["cid"] = req.cid;
    else frame["selector"] = req.selector.to_json();

    send_frame(frame);

    if (future.wait_for(std::chrono::seconds(10)) == std::future_status::ready) {
        return future.get();
    }

    {
        std::lock_guard<std::mutex> lock(node->transaction_mutex_);
        node->transactions_.erase(tx_id);
    }
    throw VFSException("WebSocket Read Timeout", 504);
}

void VFSNode::WSForwardConnection::send_frame(const json& frame) {
    // Note: This requires a persistent WS client which httplib doesn't easily expose 
    // in a non-blocking background way yet without blocking the VFS loop.
    // Implementation deferred to full IXWebSocket integration.
    std::cerr << "[WS] Forward READ not yet implemented for Native C++" << std::endl;
}

void VFSNode::WSReverseConnection::send_frame(const json& frame) {
    if (ws) {
        std::string s = frame.dump();
        ws->send(s.c_str(), s.length());
    }
}

void VFSNode::resolve_transaction(const std::string& tx_id, const VFSResult& result) {
    std::shared_ptr<std::promise<VFSResult>> p;
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);
        if (transactions_.count(tx_id)) {
            p = transactions_[tx_id];
            transactions_.erase(tx_id);
        }
    }
    if (p) p->set_value(result);
}

void VFSNode::reject_transaction(const std::string& tx_id, int status, const std::string& error) {
    std::shared_ptr<std::promise<VFSResult>> p;
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);
        if (transactions_.count(tx_id)) {
            p = transactions_[tx_id];
            transactions_.erase(tx_id);
        }
    }
    if (p) p->set_exception(std::make_exception_ptr(VFSException(error, status)));
}

} // namespace fs
