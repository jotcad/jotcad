#include "vfs.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <set>

namespace fs {

/**
 * VFSRequest Implementation for Reverse Connection
 */
class ReverseRequest : public VFSRequest {
public:
    ReverseRequest(std::function<void(int, const char*, const uint8_t*, size_t)> respond) : respond_(respond) {}
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

void ReverseConnection::notify(const json& selector, const json& payload, const std::string& source_id) {
    std::string path = selector.value("path", "unknown");
    Serial.printf("[Mesh OUT] -> NOTIFY: %s to %s\n", path.c_str(), id_.c_str());
    node_->trigger_activity();

    WiFiClient client;
    HTTPClient http;
    http.begin(client, (url_ + "/notify").c_str());
    
    std::string sel_json = selector.dump();
    http.addHeader("X-VFS-Op", "PUB");
    http.addHeader("X-VFS-Selector", sel_json.c_str());
    http.addHeader("X-VFS-Stack", source_id.c_str());
    http.addHeader("X-VFS-Encoding", "json");
    http.addHeader("Content-Type", "application/json");

    int code = http.POST(payload.dump().c_str());
    if (code != 200) {
        Serial.printf("[ReverseConn %s] Notify failed: %d\n", id_.c_str(), code);
    }
    http.end();
}

void ReverseConnection::notify_binary(const json& selector, const uint8_t* data, size_t len, const std::string& source_id) {
    std::string path = selector.value("path", "unknown");
    Serial.printf("[Mesh OUT] -> NOTIFY (BINARY %d bytes): %s to %s\n", (int)len, path.c_str(), id_.c_str());
    node_->trigger_activity();

    WiFiClient client;
    HTTPClient http;
    http.begin(client, (url_ + "/notify").c_str());
    
    std::string sel_json = selector.dump();
    http.addHeader("X-VFS-Op", "PUB");
    http.addHeader("X-VFS-Selector", sel_json.c_str());
    http.addHeader("X-VFS-Stack", source_id.c_str());
    http.addHeader("X-VFS-Encoding", "bytes");
    http.addHeader("Content-Type", "application/octet-stream");

    int code = http.POST((uint8_t*)data, len);
    if (code != 200) {
        Serial.printf("[ReverseConn %s] Binary Notify failed: %d\n", id_.c_str(), code);
    }
    http.end();
}

void ReverseConnection::tick() {
    unsigned long now = millis();
    static unsigned long poll_interval = 100;
    if (now - last_poll_ < poll_interval && last_poll_ != 0) return;

    WiFiClient client;
    HTTPClient http;
    // Cooperative timeout: keep it relatively short (e.g. 5000ms) on ESP8266 to maintain main application loop responsiveness
    http.begin(client, (url_ + "/listen").c_str());
    http.addHeader("x-vfs-peer-id", node_->id().c_str());
    http.setTimeout(5000); // 5 Seconds (yielding back to main loop frequently)
    
    int code;
    if (has_reply_) {
        Serial.printf("[Mesh OUT] -> REPLY to %s (%d bytes)\n", reply_to_.c_str(), (int)reply_body_.size());
        node_->trigger_activity();
        http.addHeader("x-vfs-reply-to", reply_to_.c_str());
        http.addHeader("Content-Type", reply_content_type_.c_str());
        http.addHeader("X-VFS-Encoding", reply_encoding_.c_str());
        if (!reply_selector_b64_.empty()) http.addHeader("X-VFS-Selector", reply_selector_b64_.c_str());
        
        code = http.POST(reply_body_.data(), reply_body_.size());
        has_reply_ = false;
        reply_to_ = "";
        reply_body_.clear();
        reply_encoding_ = "";
        reply_selector_b64_ = "";
    } else {
        code = http.POST("");
    }

    last_poll_ = millis();

    if (code == 200) {
        node_->trigger_activity();
        
        // 1. Check for Sovereign Packet Headers
        String op = http.header("X-VFS-Op");
        String sel_b64 = http.header("X-VFS-Selector");
        String encoding = http.header("X-VFS-Encoding");
        String replyTo = http.header("X-VFS-Reply-To");

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

            int bodyLen = http.getSize();
            if (encoding == "bytes") {
                // Read raw binary body in watchdog-yielding cooperative stream
                WiFiClient* stream = http.getStreamPtr();
                std::vector<uint8_t> data(bodyLen);
                int read = 0;
                while (read < bodyLen) {
                    int r = stream->read(data.data() + read, bodyLen - read);
                    if (r > 0) read += r;
                    else { delay(1); if (!stream->connected()) break; } // delay(1) feeds the WDT
                }
                node_->handle_binary_command(op.c_str(), sel, data.data(), data.size(), replyTo.c_str(), [this](int code, const char* type, const uint8_t* body, size_t len) {
                    this->reply_body_.assign(body, body + len);
                    this->reply_content_type_ = type;
                    this->reply_encoding_ = "bytes";
                    this->has_reply_ = true;
                });
            } else {
                // Read JSON body
                String body = http.getString();
                json j_body = json::parse(body.c_str());
                json cmd = {{"type", op == "PUB" ? "PUB" : "COMMAND"}, {"op", op.c_str()}, {"selector", sel.to_json()}, {"payload", j_body.contains("payload") ? j_body["payload"] : j_body}};
                if (replyTo.length() > 0) cmd["id"] = replyTo.c_str();

                node_->handle_command(cmd, [this](int code, const char* type, const uint8_t* body, size_t len) {
                    this->reply_body_.assign(body, body + len);
                    this->reply_content_type_ = type;
                    this->reply_encoding_ = "json";
                    this->has_reply_ = true;
                });
            }
            poll_interval = 50;
        } else {
            // Legacy JSON Command
            String payload = http.getString();
            try {
                json cmd = json::parse(payload.c_str());
                if (cmd.contains("id")) reply_to_ = cmd["id"];
                node_->handle_command(cmd, [this](int code, const char* type, const uint8_t* body, size_t len) {
                    this->reply_body_.assign(body, body + len);
                    this->reply_content_type_ = type;
                    this->reply_encoding_ = "json";
                    this->has_reply_ = true;
                });
                poll_interval = 50;
            } catch (...) {
                Serial.printf("[ReverseConn %s] JSON Parse Error in command\n", id_.c_str());
            }
        }
    } else if (code == 204 || code == -11) {
        poll_interval = 50; 
    } else {
        Serial.printf("[ReverseConn %s] Poll failed: %d. Re-triggering discovery.\n", id_.c_str(), code);
        poll_interval = 5000;
        node_->clear_peers();
    }

    http.end();
    yield(); // Yield CPU back to network stack maintenance
}

/**
 * VFS Implementation
 */
VFS::VFS(const Config& config) : config_(config) {
}

VFS::~VFS() {
}

void VFS::begin() {
    Serial.printf("[VFS %s] Started in REVERSE Mode (No local server) for ESP8266\n", config_.id.c_str());
    
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Normally ON
#endif
    // Note: ESP8266 is single core and does not use FreeRTOS multitasking.
    // The user MUST call tick() in their main Arduino loop().
}

void VFS::tick() {
    unsigned long now = millis();

    if (has_feature(VFS_HANDSHAKE)) {
        static unsigned long last_discovery_attempt = 0;

        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (!config_.neighbors.empty() && peers_.empty()) {
            if (now - last_discovery_attempt > 5000 || last_discovery_attempt == 0) {
                Serial.printf("[VFS %s] Discovery: hunting for neighbors...\n", config_.id.c_str());
                register_with_neighbors();
                last_discovery_attempt = now;
            }
        }
    }

    // Capture peers list under lock, then iterate outside to avoid holding lock during blocking HTTP
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        current_peers = peers_;
    }

    for (auto& peer : current_peers) {
        peer->tick();
    }
    yield(); // Maintain network stack
}

void VFS::trigger_activity() {
#ifdef LED_BUILTIN
    led_state_ = !led_state_;
    digitalWrite(LED_BUILTIN, led_state_ ? HIGH : LOW);
#endif
}

void VFS::register_with_neighbors() {
    std::vector<std::string> neighbors_to_try;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        neighbors_to_try = config_.neighbors;
    }

    for (const auto& neighbor : neighbors_to_try) {
        WiFiClient client;
        HTTPClient http;
        http.begin(client, (neighbor + "/register").c_str());
        http.addHeader("Content-Type", "application/json");

        json reg = {{"id", config_.id}};
        int code = http.POST(reg.dump().c_str());
        if (code == 200) {
            String resp = http.getString();
            try {
                json body = json::parse(resp.c_str());
                std::string reachability = body.value("reachability", "DIRECT");
                std::string remote_id = body.value("id", "unknown");

                Serial.printf("[VFS %s] Registered with neighbor %s as %s. Remote ID: %s\n", 
                              config_.id.c_str(), neighbor.c_str(), reachability.c_str(), remote_id.c_str());

                if (reachability == "REVERSE") {
                    // Add Reverse connection handler
                    add_peer(std::make_shared<ReverseConnection>(this, remote_id, neighbor));
                }
            } catch (...) {
                Serial.printf("[VFS %s] Failed to parse registration response from %s\n", config_.id.c_str(), neighbor.c_str());
            }
        } else {
            Serial.printf("[VFS %s] Registration failed with %s: %d\n", config_.id.c_str(), neighbor.c_str(), code);
        }
        http.end();
    }
}

void VFS::add_peer(std::shared_ptr<Peer> peer) {
    std::lock_guard<Mutex> lock(mesh_mutex_);
    for (const auto& p : peers_) {
        if (p->id() == peer->id()) return; // Already exists
    }
    peers_.push_back(peer);
    
    // Subscribe to interested keys
    for (const auto& [path, stack_map] : interests_) {
        for (const auto& [remote_id, exp] : stack_map) {
            subscribe(interest_selectors_[path], exp, remote_id);
        }
    }
}

void VFS::clear_peers() {
    std::lock_guard<Mutex> lock(mesh_mutex_);
    peers_.clear();
}

void VFS::register_op(const std::string& path, Handler handler, const json& schema) {
    std::lock_guard<Mutex> lock(mesh_mutex_);
    handlers_[path] = handler;
    schemas_[path] = schema;
}

int VFS::notify(const json& selector, const json& payload) {
    std::string path = selector.value("path", "unknown");
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (!interests_.count(path)) return 0;
        current_peers = peers_;
    }

    int count = 0;
    for (auto& peer : current_peers) {
        if (interests_[path].count(peer->id())) {
            peer->notify(selector, payload, config_.id);
            count++;
        }
    }
    return count;
}

int VFS::notify_binary(const json& selector, const uint8_t* data, size_t len, const std::string& source_id) {
    std::string path = selector.value("path", "unknown");
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (!interests_.count(path)) return 0;
        current_peers = peers_;
    }

    int count = 0;
    for (auto& peer : current_peers) {
        if (interests_[path].count(peer->id())) {
            peer->notify_binary(selector, data, len, source_id.empty() ? config_.id : source_id);
            count++;
        }
    }
    return count;
}

int VFS::notify_binary(const json& selector, const uint8_t* data, size_t len) {
    return notify_binary(selector, data, len, config_.id);
}

void VFS::subscribe(const json& selector, long long expiresAt, const std::string& remote_id) {
    std::string path = selector.value("path", "unknown");
    Serial.printf("[Mesh OUT] -> SUBSCRIBE: %s (expires: %lld)\n", path.c_str(), expiresAt);
    
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        interests_[path][remote_id.empty() ? "local" : remote_id] = expiresAt;
        interest_selectors_[path] = selector;
        current_peers = peers_;
    }

    // Propagate subscription to all connected peers
    for (auto& peer : current_peers) {
        WiFiClient client;
        HTTPClient http;
        http.begin(client, (neighbor_url_from_peer_id(peer->id()) + "/subscribe").c_str());
        http.addHeader("Content-Type", "application/json");
        http.addHeader("x-vfs-id", config_.id.c_str());

        json body = {{"selector", selector}, {"expiresAt", expiresAt}};
        int code = http.POST(body.dump().c_str());
        if (code != 200) {
            Serial.printf("[VFS %s] Failed to subscribe peer %s: %d\n", config_.id.c_str(), peer->id().c_str(), code);
        }
        http.end();
    }
}

// Internal helper to map peer id back to its base URL
std::string VFS::neighbor_url_from_peer_id(const std::string& peer_id) {
    // Just return the first matching neighbor config URL for now
    std::lock_guard<Mutex> lock(mesh_mutex_);
    for (const auto& neighbor : config_.neighbors) {
        if (neighbor.find(peer_id) != std::string::npos) return neighbor;
    }
    return config_.neighbors.empty() ? "" : config_.neighbors[0];
}

void VFS::handle_command(const json& cmd, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    std::string type = cmd.value("type", "unknown");
    if (type == "PUB") {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        json selector = cmd.value("selector", json::object());
        std::string path = selector.value("path", "");
        json payload = cmd.value("payload", json::object());
        
        Serial.printf("[Mesh IN] <- PUB Update: %s\n", path.c_str());
        
        // Propagate to local subscribers
        if (interests_.count(path)) {
            notify(selector, payload);
        }
        respond(200, "text/plain", (const uint8_t*)"OK", 2);
    } else if (type == "COMMAND") {
        std::string op = cmd.value("op", "");
        json selector = cmd.value("selector", json::object());
        std::string path = selector.value("path", "");
        
        Serial.printf("[Mesh IN] <- COMMAND: %s on %s\n", op.c_str(), path.c_str());
        
        if (op == "READ") {
            Handler handler;
            {
                std::lock_guard<Mutex> lock(mesh_mutex_);
                if (handlers_.count(path)) handler = handlers_[path];
            }
            if (handler) {
                ReverseRequest req(respond);
                handler(selector.value("parameters", json::object()), &req);
            } else {
                respond(404, "text/plain", (const uint8_t*)"Operator Not Found", 18);
            }
        } else if (op == "SUB") {
            long long expires = cmd.value("expiresAt", 0LL);
            std::string remote_id = cmd.value("id", "");
            subscribe(selector, expires, remote_id);
            respond(200, "text/plain", (const uint8_t*)"OK", 2);
        } else {
            respond(501, "text/plain", (const uint8_t*)"Not Implemented", 15);
        }
    }
}

void VFS::handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    if (op == "PUB") {
        Serial.printf("[Mesh IN] <- Sovereign Binary PUB: %s (%zu bytes)\n", sel.path.c_str(), len);
        notify_binary(sel.to_json(), data, len, replyTo);
        respond(200, "text/plain", (const uint8_t*)"OK", 2);
    } else if (op == "READ") {
        Handler handler;
        {
            std::lock_guard<Mutex> lock(mesh_mutex_);
            if (handlers_.count(sel.path)) handler = handlers_[sel.path];
        }
        if (handler) {
            ReverseRequest req(respond);
            handler(sel.parameters, &req);
        } else {
            respond(404, "text/plain", (const uint8_t*)"Operator Not Found", 18);
        }
    }
}

} // namespace fs
