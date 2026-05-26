#include "../vfs_node.h"
#include "../cid.h"
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "../vendor/httplib.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

namespace fs {

static std::atomic<int> active_poll_threads{0};

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

    // Broadcast the updated topology to all subscribed peers in the mesh
    json sys_topo = {{"path", "sys/topo"}, {"parameters", json::object()}};
    json neighbors = json::array();
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [p_id, conn_ptr] : peers_) {
            neighbors.push_back(json{{"id", p_id}, {"reachability", "REVERSE"}});
        }
    }
    json topo_payload = {{"type", "TOPOLOGY_UPDATE"}, {"peer", config_.id}, {"neighbors", neighbors}};
    notify(sys_topo, topo_payload, {});
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
        std::cout << "[REST " << config_.id << "] -> Deliver " << type << "/" << op << " to " << peer_id << " (Queue: " << conn->queue.size() << ")" << std::endl;
        res.set_content(cmd.dump(), "application/json");
        return;
    }
    res.status = 204;
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

} // namespace fs
