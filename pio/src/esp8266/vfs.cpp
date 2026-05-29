#include "vfs.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
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

        WiFiClient client;
        HTTPClient http;
        http.begin(client, (url_ + "/notify").c_str());
        
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

        WiFiClient client;
        HTTPClient http;
        http.begin(client, (url_ + "/subscribe").c_str());
        
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

    WiFiClient client;
    HTTPClient http;
    // Cooperative timeout: keep it relatively short (e.g. 5000ms) on ESP8266 to maintain main application loop responsiveness
    http.begin(client, (url_ + "/listen").c_str());
    http.addHeader("x-vfs-peer-id", node_->id().c_str());
    
    // CRITICAL: Must collect headers before POST
    const char * headerKeys[] = {"X-VFS-Op", "X-VFS-Selector", "X-VFS-Encoding", "X-VFS-Reply-To", "X-VFS-Stack", "X-VFS-Expires"};
    http.collectHeaders(headerKeys, 6);
    http.setTimeout(5000); 
    
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

            // Parse Stack for interest tracking
            std::vector<std::string> stack;
            String stack_header = http.header("X-VFS-Stack");
            if (stack_header.length() > 0) {
                char* s = strdup(stack_header.c_str());
                char* token = strtok(s, ",");
                while (token) {
                    stack.push_back(token);
                    token = strtok(NULL, ",");
                }
                free(s);
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
                json j_body = json::object();
                if (body.length() > 0) {
                    try { j_body = json::parse(body.c_str()); } catch (...) {}
                }
                
                json cmd = {
                    {"type", op == "PUB" ? "PUB" : "COMMAND"}, 
                    {"op", op.c_str()}, 
                    {"selector", sel.to_json()}, 
                    {"payload", j_body.contains("payload") ? j_body["payload"] : j_body},
                    {"stack", stack}
                };
                if (replyTo.length() > 0) cmd["id"] = replyTo.c_str();

                node_->handle_command(cmd, [this, replyTo](int code, const char* type, const uint8_t* body, size_t len) {
                    this->reply_to_ = replyTo.c_str();
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
    // Disabled on ESP8266 to prevent pin conflict with sensors/displays on D4 (GPIO2/LED_BUILTIN)
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
            subscribe(interest_selectors_[path], exp, {remote_id});
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

int VFS::notify(const json& selector, const json& payload, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_PUBLICATION)) return 0;

    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (!interests_.count(path)) return 0;
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

    int count = 0;
    for (auto& peer : current_peers) {
        if (interests_[path].count(peer->id())) {
            trigger_activity();
            peer->send(req);
            count++;
        }
    }
    return count;
}

int VFS::notify_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_PUBLICATION)) return 0;

    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (!interests_.count(path)) return 0;
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

    int count = 0;
    for (auto& peer : current_peers) {
        if (interests_[path].count(peer->id())) {
            trigger_activity();
            peer->send(req);
            count++;
        }
    }
    return count;
}

void VFS::subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) {
    if (!has_feature(VFS_SUBSCRIPTION)) return;
    if (std::find(stack.begin(), stack.end(), config_.id) != stack.end()) return;

    Selector sel = Selector::from_json(selector);
    std::string path = sel.path;
    std::string remote_id = stack.empty() ? "local" : stack.back();
    
    bool is_new_interest = false;
    std::vector<std::shared_ptr<Peer>> current_peers;
    {
        std::lock_guard<Mutex> lock(mesh_mutex_);
        if (interests_[path].count(remote_id) == 0) is_new_interest = true;
        interests_[path][remote_id] = expiresAt;
        interest_selectors_[path] = selector;
        current_peers = peers_;
    }

    if (!is_new_interest) return;

    // Propagate local/new interest to neighbors (Stack Protection)
    VFSRequest req;
    req.op = "SUB";
    req.selector = sel;
    req.expiresAt = expiresAt;
    req.stack = stack;
    req.stack.push_back(config_.id);

    for (auto& peer : current_peers) {
        // Don't send back to anyone already in the stack
        if (std::find(stack.begin(), stack.end(), peer->id()) != stack.end()) continue;

        trigger_activity();
        peer->send(req);
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
                ReverseResponseWriter resp(respond);
                handler(selector.value("parameters", json::object()), &resp);
            } else {
                respond(404, "text/plain", (const uint8_t*)"Operator Not Found", 18);
            }
        } else if (op == "SUB") {
            long long expires = cmd.value("expiresAt", 0LL);
            std::vector<std::string> stack = cmd.value("stack", std::vector<std::string>{});
            std::string remote_id = stack.empty() ? cmd.value("id", "") : stack.back();
            Serial.printf("[Mesh IN] <- SUB %s from %s\n", selector.value("path", "").c_str(), remote_id.c_str());
            subscribe(selector, expires, {remote_id});
            respond(200, "text/plain", (const uint8_t*)"OK", 2);
        } else {
            respond(501, "text/plain", (const uint8_t*)"Not Implemented", 15);
        }
    }
}

void VFS::handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    if (op == "PUB") {
        Serial.printf("[Mesh IN] <- Sovereign Binary PUB: %s (%zu bytes)\n", sel.path.c_str(), len);
        notify_binary(sel.to_json(), data, len, {replyTo});
        respond(200, "text/plain", (const uint8_t*)"OK", 2);
    }
 else if (op == "READ") {
        Handler handler;
        {
            std::lock_guard<Mutex> lock(mesh_mutex_);
            if (handlers_.count(sel.path)) handler = handlers_[sel.path];
        }
        if (handler) {
            ReverseResponseWriter resp(respond);
            handler(sel.parameters, &resp);
        } else {
            respond(404, "text/plain", (const uint8_t*)"Operator Not Found", 18);
        }
    }
}

} // namespace fs
