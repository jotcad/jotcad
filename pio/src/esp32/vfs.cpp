#include "vfs.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <set>

namespace fs {

/**
 * VFSResponseWriter Implementation for Reverse Connection
 */
class ReverseResponseWriter : public VFSResponseWriter {
public:
    ReverseResponseWriter(std::function<void(int, const char*, const uint8_t*, size_t)> respond) : respond_(respond) {}
    void send(int code, const char* contentType, const char* body) override {
        respond_(code, contentType, (const uint8_t*)body, strlen(body));
    }
    void send_binary(int code, const char* contentType, const uint8_t* data, size_t len) override {
        respond_(code, contentType, data, len);
    }
private:
    std::function<void(int, const char*, const uint8_t*, size_t)> respond_;
};

/**
 * ReverseConnection Implementation
 */
ReverseConnection::ReverseConnection(VFS* node, const std::string& neighbor_id, const std::string& url) 
    : node_(node), id_(neighbor_id), url_(url) {
    if (url_.back() == '/') url_.pop_back();
}

void ReverseConnection::send(const VFSRequest& req) {
    if (req.op == "PUB") {
        std::string path = req.selector.path;
        bool is_binary = (req.binary_payload != nullptr);
        
        Serial.printf("[Mesh OUT] -> PUB%s: %s to %s\n", is_binary ? " (BINARY)" : "", path.c_str(), id_.c_str());
        node_->trigger_activity();

        HTTPClient http;
        http.begin((url_ + "/notify").c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        std::string sel_json = req.selector.to_json().dump();
        std::string stack_str;
        for (size_t i = 0; i < req.stack.size(); ++i) {
            stack_str += req.stack[i];
            if (i < req.stack.size() - 1) stack_str += ",";
        }

        http.addHeader("X-VFS-Op", "PUB");
        http.addHeader("X-VFS-Selector", sel_json.c_str());
        http.addHeader("X-VFS-Stack", stack_str.c_str());
        
        int code;
        if (is_binary) {
            http.addHeader("X-VFS-Encoding", "bytes");
            http.addHeader("Content-Type", "application/octet-stream");
            code = http.POST((uint8_t*)req.binary_payload, req.binary_len);
        } else {
            http.addHeader("X-VFS-Encoding", "json");
            http.addHeader("Content-Type", "application/json");
            code = http.POST(req.payload.dump().c_str());
        }

        if (code != 200) {
            Serial.printf("[ReverseConn %s] PUB failed: %d\n", id_.c_str(), code);
        }
        http.end();
    } else if (req.op == "SUB") {
        Serial.printf("[Mesh OUT] -> SUB: %s to %s\n", req.selector.path.c_str(), id_.c_str());
        node_->trigger_activity();

        HTTPClient http;
        http.begin((url_ + "/subscribe").c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        std::string sel_json = req.selector.to_json().dump();
        std::string stack_str;
        for (size_t i = 0; i < req.stack.size(); ++i) {
            stack_str += req.stack[i];
            if (i < req.stack.size() - 1) stack_str += ",";
        }

        http.addHeader("X-VFS-Op", "SUB");
        http.addHeader("X-VFS-Selector", sel_json.c_str());
        http.addHeader("X-VFS-Stack", stack_str.c_str());
        http.addHeader("X-VFS-Expires", std::to_string(req.expiresAt).c_str());
        
        int code = http.POST("");
        if (code != 200) {
            Serial.printf("[ReverseConn %s] SUB failed: %d\n", id_.c_str(), code);
        }
        http.end();
    }
}

void ReverseConnection::tick() {

    unsigned long now = millis();
    static unsigned long poll_interval = 100;
    if (now - last_poll_ < poll_interval && last_poll_ != 0) return;

    HTTPClient http;
    http.begin((url_ + "/listen").c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("x-vfs-peer-id", node_->id().c_str());
    
    const char * headerKeys[] = {"X-VFS-Op", "X-VFS-Selector", "X-VFS-Encoding", "X-VFS-Reply-To", "X-VFS-Stack", "X-VFS-Expires"};
    http.collectHeaders(headerKeys, 6);

    int code;
    if (has_reply_) {
        http.addHeader("X-VFS-Reply-To", reply_to_.c_str());
        http.addHeader("X-VFS-Encoding", reply_encoding_.c_str());
        http.addHeader("X-VFS-Selector", reply_selector_b64_.c_str());
        code = http.POST(reply_body_.data(), reply_body_.size());
        has_reply_ = false;
        reply_body_.clear();
    } else {
        code = http.GET();
    }

    if (code == 200) {
        last_poll_ = millis();
        std::string op = http.header("X-VFS-Op").c_str();
        std::string sel_b64 = http.header("X-VFS-Selector").c_str();
        std::string encoding = http.header("X-VFS-Encoding").c_str();
        std::string reply_to = http.header("X-VFS-Reply-To").c_str();
        std::string stack_str = http.header("X-VFS-Stack").c_str();

        if (op.length() > 0) {
            Serial.printf("[Mesh IN] <- Sovereign %s from %s\n", op.c_str(), id_.c_str());
            Selector sel;
            if (sel_b64.length() > 0) {
                try {
                    // Try parsing as JSON first
                    sel = Selector::from_json(json::parse(sel_b64.c_str()));
                } catch (...) {
                    // Fallback to JCB/Base64
                    try { sel = Selector::from_json(decode_jcb(base64_decode(sel_b64.c_str()))); } catch (...) {}
                }
            }

            // Parse Stack for interest tracking
            std::vector<std::string> stack;
            size_t pos = 0;
            while ((pos = stack_str.find(",")) != std::string::npos) {
                stack.push_back(stack_str.substr(0, pos));
                stack_str.erase(0, pos + 1);
            }
            if (stack_str.length() > 0) stack.push_back(stack_str);

            auto respond = [this, reply_to, sel](int code, const char* ct, const uint8_t* data, size_t len) {
                this->reply_to_ = reply_to;
                this->reply_body_.assign(data, data + len);
                this->reply_content_type_ = ct;
                this->reply_encoding_ = (strstr(ct, "json") ? "json" : "bytes");
                this->reply_selector_b64_ = base64_encode(encode_jcb(sel.to_json()));
                this->has_reply_ = true;
            };

            if (encoding == "bytes") {
                int len = http.getSize();
                std::vector<uint8_t> body(len);
                http.getStream().readBytes(body.data(), len);
                node_->handle_binary_command(op, sel, body.data(), len, reply_to, respond);
            } else {
                json body = json::parse(http.getString().c_str());
                if (op == "PUB") {
                    node_->handle_command(body, respond);
                } else {
                    // COMMAND (READ/SUB/etc)
                    json cmd = {{"type", "COMMAND"}, {"op", op}, {"selector", sel.to_json()}, {"stack", stack}};
                    if (http.hasHeader("X-VFS-Expires")) cmd["expiresAt"] = atoll(http.header("X-VFS-Expires").c_str());
                    node_->handle_command(cmd, respond);
                }
            }
        }
    }
    http.end();
}

/**
 * WSConnection Implementation
 */
WSConnection::WSConnection(VFS* node, const std::string& neighbor_id, const std::string& url) 
    : node_(node), id_(neighbor_id), url_(url) {
    
    std::string host = url_;
    int port = 80;
    std::string path = "/";

    if (host.find("ws://") == 0) {
        host = host.substr(5);
    } else if (host.find("wss://") == 0) {
        host = host.substr(6);
        port = 443;
    }

    size_t path_pos = host.find("/");
    if (path_pos != std::string::npos) {
        path = host.substr(path_pos);
        host = host.substr(0, path_pos);
    }

    size_t port_pos = host.find(":");
    if (port_pos != std::string::npos) {
        port = std::stoi(host.substr(port_pos + 1));
        host = host.substr(0, port_pos);
    }

    Serial.printf("[WS %s] Connecting to %s:%d%s\n", id_.c_str(), host.c_str(), port, path.c_str());
    client_.begin(host.c_str(), port, path.c_str());
    client_.onEvent([this](WStype_t type, uint8_t * payload, size_t length) {
        this->on_event(type, payload, length);
    });
    client_.setReconnectInterval(5000);
}

WSConnection::~WSConnection() {
    client_.disconnect();
}

void WSConnection::on_event(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.printf("[WS %s] Disconnected!\n", id_.c_str());
            connected_ = false;
            break;
        case WStype_CONNECTED:
            Serial.printf("[WS %s] Connected!\n", id_.c_str());
            connected_ = true;
            client_.sendTXT(json({{"type", "IDENTIFY"}, {"peerId", node_->id()}}).dump().c_str());
            break;
        case WStype_TEXT: {
            std::string msg((char*)payload, length);
            try {
                json frame = json::parse(msg);
                if (frame.value("hasBinary", false)) {
                    expecting_binary_header_ = frame;
                } else {
                    node_->handle_ws_frame(frame, this);
                }
            } catch (...) {}
            break;
        }
        case WStype_BIN: {
            if (!expecting_binary_header_.empty()) {
                std::string op = expecting_binary_header_.value("type", "");
                Selector sel = Selector::from_json(expecting_binary_header_.value("selector", json::object()));
                if (op == "NOTIFY" || op == "PUB") {
                    node_->handle_binary_command("PUB", sel, payload, length, id_, [](int, const char*, const uint8_t*, size_t){});
                }
                expecting_binary_header_ = json::object();
            }
            break;
        }
        default: break;
    }
}

void WSConnection::send(const VFSRequest& req) {
    if (!connected_) return;
    json frame = {
        {"type", req.op == "PUB" ? "NOTIFY" : (req.op == "SUB" ? "SUBSCRIBE" : req.op)},
        {"selector", req.selector.to_json()},
        {"stack", req.stack},
        {"expiresAt", req.expiresAt}
    };
    if (!req.txId.empty()) frame["txId"] = req.txId;
    if (req.op == "READ_RESPONSE") {
        frame["status"] = req.payload.value("status", 200);
        if (req.payload.contains("metadata")) frame["metadata"] = req.payload["metadata"];
    }

    if (req.binary_payload) {
        frame["hasBinary"] = true;
        client_.sendTXT(frame.dump().c_str());
        client_.sendBIN((uint8_t*)req.binary_payload, req.binary_len);
    } else {
        if (!req.payload.empty()) frame["payload"] = req.payload;
        client_.sendTXT(frame.dump().c_str());
    }
}

void WSConnection::tick() {
    client_.loop();
}

/**
 * VFS Implementation
 */
VFS::VFS(const Config& config) : config_(config) {}

VFS::~VFS() {}

void VFS::begin() {
}

void VFS::tick() {
    static unsigned long last_activity = 0;
    if (millis() - last_activity > 100) {
        last_activity = millis();
    }

    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        current_peers = peers_;
    }
    for (auto& peer : current_peers) {
        peer->tick();
    }
}

void VFS::trigger_activity() {
    led_state_ = !led_state_;
}

void VFS::register_with_neighbors() {
    for (const auto& neighbor : config_.neighbors) {
        HTTPClient http;
        http.begin((neighbor + "/register").c_str());
        http.addHeader("Content-Type", "application/json");
        
        json body = {{"id", config_.id}, {"url", ""}}; 
        int code = http.POST(body.dump().c_str());

        if (code == 200) {
            json resp = json::parse(http.getString().c_str());
            std::string neighbor_id = resp.value("id", neighbor);

            Serial.printf("[VFS %s] Registered successfully with %s (Neighbor ID: %s, Reachability: %s)\n", 
                          config_.id.c_str(), neighbor.c_str(), neighbor_id.c_str(), resp["reachability"].get<std::string>().c_str());

            // WS Upgrade phase
            bool upgraded = false;
            if (resp.contains("wsUrl")) {
                std::string wsUrl = resp["wsUrl"];
                auto ws_conn = std::make_shared<WSConnection>(this, neighbor_id, wsUrl);
                add_peer(ws_conn);
                upgraded = true;
                Serial.printf("[VFS %s] Upgraded peer %s to WebSocket\n", config_.id.c_str(), neighbor_id.c_str());
            }

            if (!upgraded) {
                add_peer(std::make_shared<ReverseConnection>(this, neighbor_id, neighbor));
            }
        } else {
            Serial.printf("[VFS %s] Registration failed for %s: %s (%d)\n", 
                          config_.id.c_str(), neighbor.c_str(), http.errorToString(code).c_str(), code);
        }
        http.end();
    }
}

void VFS::add_peer(std::shared_ptr<Peer> peer) {
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        peers_.push_back(peer);
    }
    
    json neighbors = json::array();
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        for (auto const& p : peers_) {
            neighbors.push_back({
                {"id", p->id()},
                {"reachability", p->reachability()},
                {"protocol", p->protocol()}
            });
        }
    }
    notify({{"path", "sys/topo"}}, {{"peer", config_.id}, {"neighbors", neighbors}});
}

void VFS::upgrade_peer_to_ws(const std::string& peer_id, std::shared_ptr<Peer> ws_conn) {
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        for (auto it = peers_.begin(); it != peers_.end(); ++it) {
            if ((*it)->id() == peer_id) {
                peers_.erase(it);
                break;
            }
        }
        peers_.push_back(ws_conn);
    }
    Serial.printf("[VFS] Peer %s upgraded to WebSocket!\n", peer_id.c_str());

    json neighbors = json::array();
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        for (auto const& p : peers_) {
            neighbors.push_back({
                {"id", p->id()},
                {"reachability", p->reachability()},
                {"protocol", p->protocol()}
            });
        }
    }
    notify({{"path", "sys/topo"}}, {{"peer", config_.id}, {"neighbors", neighbors}});
}

void VFS::clear_peers() {
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    peers_.clear();
}

void VFS::register_op(const std::string& path, Handler handler, const json& schema) {
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFS::handle_command(const json& cmd, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    if (!cmd.contains("type")) return;
    std::string type = cmd.at("type");
    
    if (type == "COMMAND") {
        std::string op = cmd.at("op");
        if (op == "READ" && has_feature(VFS_FULFILLMENT)) {
            Selector sel = Selector::from_json(cmd.at("selector"));
            Serial.printf("[VFS] Processing READ: %s\n", sel.path.c_str());
            trigger_activity();
            if (handlers_.count(sel.path)) {
                ReverseResponseWriter resp(respond);
                handlers_[sel.path](sel.parameters, &resp);
            } else {
                respond(404, "text/plain", (const uint8_t*)"Not Found", 9);
            }
        } else if (op == "SUB" && has_feature(VFS_SUBSCRIPTION)) {
            Selector sel = Selector::from_json(cmd.at("selector"));
            long long expiresAt = cmd.at("expiresAt");
            std::string neighbor_id = cmd.at("stack").back();
            Serial.printf("[VFS] Processing SUB: %s from %s\n", sel.path.c_str(), neighbor_id.c_str());
            trigger_activity();
            this->subscribe(sel.to_json(), expiresAt, {neighbor_id});
        }
    } else if (type == "PUB") {
        Selector sel = Selector::from_json(cmd.at("selector"));
        Serial.printf("[Mesh IN] <- PUB: %s\n", sel.path.c_str());
        trigger_activity();
    }
}

class WSResponseWriter : public VFSResponseWriter {
public:
    WSResponseWriter(WSConnection* conn, const std::string& txId) : conn_(conn), txId_(txId) {}
    void send(int code, const char* contentType, const char* body) override {
        VFSRequest req;
        req.op = "READ_RESPONSE";
        req.txId = txId_;
        req.payload = {{"status", code}};
        try { req.payload["data"] = json::parse(body); } catch(...) { req.payload["data"] = body; }
        conn_->send(req);
    }
    void send_binary(int code, const char* contentType, const uint8_t* data, size_t len) override {
        VFSRequest req;
        req.op = "READ_RESPONSE";
        req.txId = txId_;
        req.payload = {{"status", code}};
        req.binary_payload = data;
        req.binary_len = len;
        conn_->send(req);
    }
private:
    WSConnection* conn_;
    std::string txId_;
};

void VFS::handle_ws_frame(const json& frame, WSConnection* conn) {
    std::string type = frame.value("type", "");
    if (type == "READ" || type == "READ_SELECTOR" || type == "READ_CID") {
        Selector sel = Selector::from_json(frame.value("selector", json::object()));
        std::string tx_id = frame.value("txId", "");
        if (handlers_.count(sel.path)) {
            WSResponseWriter resp(conn, tx_id);
            handlers_[sel.path](sel.parameters, &resp);
        }
    } else if (type == "NOTIFY" || type == "PUB") {
        handle_command(frame, [](int, const char*, const uint8_t*, size_t){});
    } else if (type == "SUBSCRIBE" || type == "SUB") {
        handle_command(frame, [](int, const char*, const uint8_t*, size_t){});
    }
}

void VFS::handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    if (op == "PUB") {
        Serial.printf("[Mesh IN] <- Sovereign Binary PUB: %s (%d bytes)\n", sel.path.c_str(), (int)len);
        trigger_activity();
    }
}

int VFS::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_PUBLICATION)) return 0;

    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::set<std::string> targets;

    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        if (interests_.count(path)) {
            for (auto const& [peer_id, expiry] : interests_[path]) {
                if (expiry > millis()) targets.insert(peer_id);
            }
        }
    }

    int notified = 0;
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        current_peers = peers_;
    }

    VFSRequest req;
    req.op = "PUB";
    req.selector = sel;
    req.payload = payload;
    req.stack = stack;
    if (req.stack.empty()) req.stack.push_back(config_.id);
    else if (std::find(req.stack.begin(), req.stack.end(), config_.id) == req.stack.end()) {
        req.stack.push_back(config_.id);
    }

    for (const auto& target_id : targets) {
        for (auto& peer : current_peers) {
            if (peer->id() == target_id) {
                trigger_activity();
                peer->send(req);
                notified++;
            }
        }
    }
    return notified;
}

int VFS::notify_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_PUBLICATION)) return 0;

    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::set<std::string> targets;

    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        if (interests_.count(path)) {
            for (auto const& [peer_id, expiry] : interests_[path]) {
                if (expiry > millis()) targets.insert(peer_id);
            }
        }
    }

    int notified = 0;
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        current_peers = peers_;
    }

    VFSRequest req;
    req.op = "PUB";
    req.selector = sel;
    req.binary_payload = data;
    req.binary_len = len;
    req.stack = stack;
    if (req.stack.empty()) req.stack.push_back(config_.id);
    else if (std::find(req.stack.begin(), req.stack.end(), config_.id) == req.stack.end()) {
        req.stack.push_back(config_.id);
    }

    for (const auto& target_id : targets) {
        for (auto& peer : current_peers) {
            if (peer->id() == target_id) {
                trigger_activity();
                peer->send(req);
                notified++;
            }
        }
    }
    return notified;
}

void VFS::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_SUBSCRIPTION)) return;
    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::string remote_id = stack.empty() ? "" : stack.back();

    {
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        interests_[path][remote_id] = expiresAt;
        interest_selectors_[path] = selector;
    }

    Serial.printf("[VFS %s] Subscription recorded for %s from %s (expires in %lldms)\n", 
                  config_.id.c_str(), path.c_str(), remote_id.c_str(), expiresAt - millis());

    VFSRequest req;
    req.op = "SUB";
    req.selector = sel;
    req.expiresAt = expiresAt;
    req.stack = stack;
    if (std::find(req.stack.begin(), req.stack.end(), config_.id) == req.stack.end()) {
        req.stack.push_back(config_.id);
    }

    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    for (auto& peer : peers_) {
        if (peer->id() != remote_id) {
            trigger_activity();
            peer->send(req);
        }
    }
}

} // namespace fs
