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

json VFSNode::get_topology_payload() {
    json rich_neighbors = json::array();
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [p_id, conn_ptr] : peers_) {
            rich_neighbors.push_back(json{
                {"id", p_id}, 
                {"reachability", conn_ptr->is_reverse() ? "REVERSE" : "DIRECT"},
                {"protocol", conn_ptr->get_protocol()}
            });
        }
    }

    json activeInterests = json::array();
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (auto const& entry : interests_) {
            bool is_local = entry.localExpiresAt > now;
            bool has_remote = false;
            for (auto const& [neighbor_id, expiry] : entry.subs) {
                if (expiry > now) {
                    has_remote = true;
                    break;
                }
            }
            if (is_local || has_remote) {
                activeInterests.push_back({
                    {"path", entry.selector.path},
                    {"local", is_local},
                    {"remote", has_remote}
                });
            }
        }
    }

    return {{"id", config_.id}, {"neighbors", rich_neighbors}, {"interests", activeInterests}};
}

void VFSNode::add_connection(std::shared_ptr<Connection> conn) {
    if (!conn) return;
    auto ws_conn = std::dynamic_pointer_cast<WSConnection>(conn);
    if (ws_conn) {
        ws_conn->node = this;
    }
    std::string id = conn->neighbor_id;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peers_[id] = conn;
    }
    std::cout << "[VFSNode " << config_.id << "] Peer Added: " << id << (conn->is_reverse() ? " (REVERSE)" : " (DIRECT)") << std::endl;
    
    {
        std::lock_guard<std::mutex> lock(interest_mutex_);
        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (auto const& entry : interests_) {
            long long maxExp = entry.localExpiresAt;
            for (auto const& [neighbor_id, expiry] : entry.subs) if (expiry > maxExp) maxExp = expiry;
            if (maxExp > now) {
                VFSRequest req;
                req.op = "SUB";
                req.selector = entry.selector;
                req.expiresAt = maxExp;
                req.stack = {config_.id};
                conn->sendRequest(req);
            }
        }
    }

    json sys_topo = {{"path", "sys/topo"}, {"parameters", json::object()}};
    notify(sys_topo, get_topology_payload(), {});
}

void VFSNode::upgrade_peer_to_ws(const std::string& peer_id, std::shared_ptr<Connection> ws_conn) {
    if (!ws_conn) return;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        peers_[peer_id] = ws_conn;
    }
    std::cout << "[VFSNode " << config_.id << "] Peer " << peer_id << " upgraded to WebSocket." << std::endl;
    add_connection(ws_conn);
}

void VFSNode::add_peer(const std::string& url) {
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        for (auto const& [p_id, conn_ptr] : peers_) {
            if (conn_ptr->get_url() == url) {
                return;
            }
        }
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
        PollThreadGuard(std::atomic<int>& c) : count(c) {}
        ~PollThreadGuard() { --count; }
    };

    int current_active = ++active_poll_threads;
    if (current_active >= 8) {
        std::cout << "[VFSNode " << config_.id << "] !!! THREAD EXHAUSTION WARNING !!! "
                  << "Total active/blocked long-polling C++ threads: " << current_active << "/8." << std::endl;
    }

    PollThreadGuard guard(active_poll_threads);

    std::shared_ptr<ReverseConnection> conn;
    {
        std::lock_guard<std::mutex> lock(peer_mutex_);
        if (peers_.count(peer_id)) conn = std::dynamic_pointer_cast<ReverseConnection>(peers_[peer_id]);
    }

    if (!conn) { res.status = 404; return; }

    std::unique_lock<std::mutex> lock(conn->mutex);
    if (conn->is_polling) {
        conn->current_poll_id++;
        conn->cv.notify_all();
        conn->cv.wait_for(lock, std::chrono::seconds(2), [&conn] { return !conn->is_polling; });
    }

    long long my_poll_id = ++conn->current_poll_id;
    conn->is_polling = true;

    if (conn->queue.empty()) {
        conn->cv.wait(lock, [&conn, my_poll_id] {
            return !conn->queue.empty() || conn->current_poll_id != my_poll_id;
        });
    }

    conn->is_polling = false;
    conn->cv.notify_all();

    if (conn->current_poll_id != my_poll_id) { res.status = 204; return; }

    if (!conn->queue.empty()) {
        // BINARY RULE: If the first item is binary, deliver it alone as a standard response
        if (conn->queue.front().contains("payload") && conn->queue.front()["payload"].is_binary()) {
            json cmd = conn->queue.front();
            conn->queue.erase(conn->queue.begin());
            std::string op = cmd.value("op", cmd.value("type", "unknown"));
            
            res.set_header("X-VFS-Op", op);
            if (cmd.contains("selector")) res.set_header("X-VFS-Selector", cmd["selector"].dump());
            if (cmd.contains("expiresAt")) res.set_header("X-VFS-Expires", std::to_string(cmd["expiresAt"].get<long long>()));
            
            std::string stack_str;
            if (cmd.contains("stack")) {
                for (size_t i = 0; i < cmd["stack"].size(); ++i) stack_str += (i == 0 ? "" : ",") + cmd["stack"][i].get<std::string>();
            }
            res.set_header("X-VFS-Stack", stack_str);
            
            res.set_header("X-VFS-Encoding", "bytes");
            std::vector<uint8_t> data = cmd["payload"].get_binary();
            res.set_content((const char*)data.data(), data.size(), "application/octet-stream");
            return;
        }

        // BATCH RULE: Flush all non-binary messages in one array
        json batch = json::array();
        while (!conn->queue.empty() && 
               !(conn->queue.front().contains("payload") && conn->queue.front()["payload"].is_binary())) {
            batch.push_back(conn->queue.front());
            conn->queue.erase(conn->queue.begin());
        }
        
        std::cout << "[REST " << config_.id << "] -> Deliver BATCH of " << batch.size() << " commands to " << peer_id << std::endl;
        
        res.set_header("X-VFS-Op", "BATCH");
        res.set_header("X-VFS-Encoding", "json");
        res.set_content(batch.dump(), "application/json");
        return;
    }
    res.status = 204;
}

VFSResult VFSNode::ForwardConnection::_do_read(const std::map<std::string, std::string>& headers, const std::string& body, const std::string& path) {
    httplib::Client cli(url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    cli.enable_server_certificate_verification(false);
#endif
    httplib::Headers h;
    for (auto const& [k, v] : headers) h.emplace(k, v);

    if (auto res = cli.Post(path, h, body, "application/json")) {
        if (res->status == 200) {
            VFSResult result;
            result.data = std::vector<uint8_t>(res->body.begin(), res->body.end());
            if (res->has_header("x-vfs-info")) {
                std::string info_raw = res->get_header_value("x-vfs-info");
                try {
                    if (!info_raw.empty() && info_raw[0] == '{') result.metadata = json::parse(info_raw);
                    else {
                        std::vector<uint8_t> info_bytes = fs::base64_decode(info_raw);
                        result.metadata = json::parse(std::string(info_bytes.begin(), info_bytes.end()));
                    }
                } catch(...) { result.metadata = {{"state", "AVAILABLE"}}; }
            } else result.metadata = {{"state", "AVAILABLE"}};
            return result;
        } else throw VFSException(res->body, res->status);
    }
    throw VFSException("Network failure", 503);
}

VFSResult VFSNode::ForwardConnection::sendRequest(const VFSRequest& req) {
    if (req.op == "PUB") {
        httplib::Headers headers = {
            {"X-VFS-Op", "PUB"},
            {"X-VFS-Selector", req.selector.to_json().dump()},
            {"X-VFS-Stack", ""},
            {"X-VFS-Encoding", "json"},
            {"Content-Type", "application/json"}
        };
        for (size_t i = 0; i < req.stack.size(); ++i) headers.find("X-VFS-Stack")->second += (i == 0 ? "" : ",") + req.stack[i];

        std::string body = req.data;
        std::string target_url = url;
        std::thread([target_url, body, headers]() {
            httplib::Client cli(target_url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            cli.enable_server_certificate_verification(false);
#endif
            cli.Post("/notify", headers, body, "application/json");
        }).detach();
        return {};
    } else if (req.op == "SUB") {
        httplib::Headers headers = {
            {"X-VFS-Op", "SUB"},
            {"X-VFS-Selector", req.selector.to_json().dump()},
            {"X-VFS-Expires", std::to_string(req.expiresAt)},
            {"X-VFS-Stack", ""}
        };
        for (size_t i = 0; i < req.stack.size(); ++i) headers.find("X-VFS-Stack")->second += (i == 0 ? "" : ",") + req.stack[i];

        std::string target_url = url;
        std::thread([target_url, headers]() {
            httplib::Client cli(target_url);
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            cli.enable_server_certificate_verification(false);
#endif
            cli.Post("/subscribe", headers, "", "application/json");
        }).detach();
        return {};
    } else if (req.op == "READ_SELECTOR" || req.op == "READ_CID") {
        std::map<std::string, std::string> headers = {
            {"X-VFS-Op", req.op},
            {"X-VFS-Stack", ""},
            {"X-VFS-Expires", std::to_string(req.expiresAt)}
        };
        for (size_t i = 0; i < req.stack.size(); ++i) headers["X-VFS-Stack"] += (i == 0 ? "" : ",") + req.stack[i];
        
        std::string path;
        std::string body = "";
        
        if (req.op == "READ_SELECTOR") {
            headers["X-VFS-Selector"] = req.selector.to_json().dump();
            path = "/read_selector";
        } else {
            path = "/read_cid";
            json jbody = {{"cid", req.cid}, {"resolutionStack", req.resolutionStack}};
            body = jbody.dump();
        }
        
        return _do_read(headers, body, path);
    }
    throw VFSException("ForwardConnection sendRequest unsupported op: " + req.op, 501);
}

VFSResult VFSNode::ReverseConnection::sendRequest(const VFSRequest& req) {
    if (req.op == "PUB") {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push_back({{"type", "PUB"}, {"selector", req.selector.to_json()}, {"payload", req.data.empty() ? json::object() : json::parse(req.data)}, {"stack", req.stack}});
        std::cout << "[ReverseConn " << neighbor_id << "] PUB queued. Queue depth: " << queue.size() << std::endl;
        cv.notify_one();
        return {};
    } else if (req.op == "SUB") {
        std::lock_guard<std::mutex> lock(mutex);
        queue.push_back({{"type", "COMMAND"}, {"op", "SUB"}, {"selector", req.selector.to_json()}, {"expiresAt", req.expiresAt}, {"stack", req.stack}});
        cv.notify_one();
        return {};
    } else if (req.op == "READ_SELECTOR" || req.op == "READ_CID") {
        std::lock_guard<std::mutex> lock(mutex);
        json cmd = {
            {"type", "COMMAND"}, 
            {"op", req.op}, 
            {"id", generate_tx_id()},
            {"expiresAt", req.expiresAt}, 
            {"stack", req.stack}
        };
        if (req.op == "READ_SELECTOR") {
            cmd["selector"] = req.selector.to_json();
        } else {
            cmd["cid"] = req.cid;
            cmd["resolutionStack"] = req.resolutionStack;
        }
        queue.push_back(cmd);
        cv.notify_one();

        // Note: Reverse READ over Long-Poll is technically 'fire and forget' 
        // until the next poll returns the reply.
        return {};
    }
    throw VFSException("ReverseConnection sendRequest " + req.op + " not yet implemented", 501);

}

// --- WebSocket Connection Implementations ---

VFSResult VFSNode::WSConnection::_do_ws_read(const json& frame) {
    std::string tx_id = frame["txId"];
    auto promise = std::make_shared<std::promise<VFSResult>>();
    auto future = promise->get_future();
    {
        std::lock_guard<std::mutex> lock(node->transaction_mutex_);
        node->transactions_[tx_id] = promise;
    }
    send_frame(frame);
    if (future.wait_for(std::chrono::seconds(15)) == std::future_status::ready) return future.get();
    {
        std::lock_guard<std::mutex> lock(node->transaction_mutex_);
        node->transactions_.erase(tx_id);
    }
    throw VFSException("WebSocket Read Timeout", 504);
}

VFSResult VFSNode::WSConnection::sendRequest(const VFSRequest& req) {
    if (req.op == "PUB") {
        if (!req.binary_data.empty()) {
            send_binary_frame({{"type", "PUB"}, {"selector", req.selector.to_json()}, {"stack", req.stack}, {"hasBinary", true}}, req.binary_data);
        } else {
            json payload = req.data.empty() ? json::object() : json::parse(req.data);
            send_frame({{"type", "PUB"}, {"selector", req.selector.to_json()}, {"payload", payload}, {"stack", req.stack}});
        }
        return {};
    } else if (req.op == "SUB") {
        send_frame({{"type", "SUB"}, {"selector", req.selector.to_json()}, {"expiresAt", req.expiresAt}, {"stack", req.stack}});
        return {};
    }
 else if (req.op == "READ_SELECTOR" || req.op == "READ_CID") {
        json frame = {
            {"type", req.op},
            {"txId", generate_tx_id()},
            {"stack", req.stack},
            {"expiresAt", req.expiresAt}
        };
        if (req.op == "READ_SELECTOR") {
            frame["selector"] = req.selector.to_json();
        } else {
            frame["cid"] = req.cid;
            frame["resolutionStack"] = req.resolutionStack;
        }
        return _do_ws_read(frame);
    }
    throw VFSException("WSConnection sendRequest unsupported op: " + req.op, 501);
}

VFSNode::WSForwardConnection::WSForwardConnection(std::string id, std::string u) {
    neighbor_id = std::move(id);
    url = std::move(u);
}

VFSNode::WSForwardConnection::~WSForwardConnection() {
    is_closed = true;
    if (ws_client) {
        ws_client->close();
    }
    if (read_thread.joinable()) {
        read_thread.join();
    }
}

void VFSNode::WSForwardConnection::start_client() {
    if (ws_client && ws_client->is_open()) return;
    
    is_closed = false;
    std::string ws_url = url;
    if (ws_url.rfind("http://", 0) == 0) {
        ws_url.replace(0, 7, "ws://");
    } else if (ws_url.rfind("https://", 0) == 0) {
        ws_url.replace(0, 8, "wss://");
    }
    
    if (ws_url.find("/vfs-ws") == std::string::npos) {
        if (ws_url.back() == '/') {
            ws_url += "vfs-ws";
        } else {
            ws_url += "/vfs-ws";
        }
    }

    std::cout << "[WS Client] Connecting to " << ws_url << "..." << std::endl;
    ws_client = std::make_shared<httplib::ws::WebSocketClient>(ws_url);
    ws_client->set_connection_timeout(2);
    ws_client->set_read_timeout(10);
    
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    ws_client->enable_server_certificate_verification(false);
#endif

    if (!ws_client->connect()) {
        std::cerr << "[WS Client] Connection failed to " << ws_url << std::endl;
        ws_client.reset();
        return;
    }
    
    std::cout << "[WS Client] Handshake successful! Sending IDENTIFY frame." << std::endl;
    json identify = {{"type", "IDENTIFY"}, {"peerId", node ? node->config_.id : "unknown"}};
    ws_client->send(identify.dump());
    
    if (read_thread.joinable()) {
        read_thread.join();
    }
    read_thread = std::thread(&VFSNode::WSForwardConnection::read_loop, this);
}

void VFSNode::WSForwardConnection::send_frame(const json& frame) {
    start_client();
    std::lock_guard<std::mutex> lock(ws_mutex);
    if (ws_client && ws_client->is_open()) {
        ws_client->send(frame.dump());
    } else {
        std::cerr << "[WS Client] Cannot send frame, client not connected." << std::endl;
    }
}

void VFSNode::WSForwardConnection::send_binary_frame(const json& header, const std::vector<uint8_t>& data) {
    start_client();
    std::lock_guard<std::mutex> lock(ws_mutex);
    if (ws_client && ws_client->is_open()) {
        ws_client->send(header.dump());
        ws_client->send((const char*)data.data(), data.size());
    } else {
        std::cerr << "[WS Client] Cannot send binary frame, client not connected." << std::endl;
    }
}

void VFSNode::WSForwardConnection::read_loop() {
    std::string msg;
    json expecting_binary_header;
    while (!is_closed && ws_client && ws_client->is_open()) {
        auto res = ws_client->read(msg);
        if (res == httplib::ws::ReadResult::Fail) {
            std::cerr << "[WS Client] Read failed or connection closed." << std::endl;
            break;
        }

        if (res == httplib::ws::ReadResult::Binary) {
            if (expecting_binary_header.empty()) {
                std::cerr << "[WS Client] Unexpected binary frame, ignoring." << std::endl;
                continue;
            }
            if (node) {
                std::vector<uint8_t> data(msg.begin(), msg.end());
                node->handle_binary_frame(neighbor_id, expecting_binary_header, data);
            }
            expecting_binary_header.clear();
        } else {
            try {
                json frame = json::parse(msg);
                if (frame.value("hasBinary", false)) {
                    expecting_binary_header = frame;
                } else if (node) {
                    node->handle_ws_frame(neighbor_id, frame);
                }
            } catch (...) {
                std::cerr << "[WS Client] Failed to parse JSON frame: " << msg << std::endl;
            }
        }
    }
}

void VFSNode::WSReverseConnection::send_frame(const json& frame) {
    std::lock_guard<std::mutex> lock(ws_mutex);
    if (ws) { std::string s = frame.dump(); ws->send(s); }
}

void VFSNode::WSReverseConnection::send_binary_frame(const json& header, const std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(ws_mutex);
    if (ws) {
        ws->send(header.dump());
        ws->send((const char*)data.data(), data.size());
    }
}

void VFSNode::resolve_transaction(const std::string& tx_id, const VFSResult& result) {
    std::shared_ptr<std::promise<VFSResult>> p;
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);
        if (transactions_.count(tx_id)) { p = transactions_[tx_id]; transactions_.erase(tx_id); }
    }
    if (p) p->set_value(result);
}

void VFSNode::reject_transaction(const std::string& tx_id, int status, const std::string& error) {
    std::shared_ptr<std::promise<VFSResult>> p;
    {
        std::lock_guard<std::mutex> lock(transaction_mutex_);
        if (transactions_.count(tx_id)) { p = transactions_[tx_id]; transactions_.erase(tx_id); }
    }
    if (p) p->set_exception(std::make_exception_ptr(VFSException(error, status)));
}

} // namespace fs
