#include "vfs.h"
#include <WiFi.h>
#include <HTTPClient.h>
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
                // Read raw binary body
                WiFiClient* stream = http.getStreamPtr();
                std::vector<uint8_t> data(bodyLen);
                int read = 0;
                while (read < bodyLen) {
                    int r = stream->read(data.data() + read, bodyLen - read);
                    if (r > 0) read += r;
                    else { delay(1); if (!stream->connected()) break; }
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
}

/**
 * VFS Implementation
 */
VFS::VFS(const Config& config) : config_(config) {
}

VFS::~VFS() {
}

// Background Task for Mesh Listeners
void vfs_mesh_task(void* parameter) {
    VFS* node = (VFS*)parameter;
    Serial.println("\n[VFS] Mesh Task Started on Core 0");
    Serial.flush();
    while (true) {
        node->tick();
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield
    }
}

void VFS::begin() {
    Serial.printf("[VFS %s] Started in REVERSE Mode (No local server)\n", config_.id.c_str());
    
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // Normally ON
#endif

    // Start Mesh Task on Core 0 (leaving Core 1 for sensors/main loop)
    xTaskCreatePinnedToCore(
        vfs_mesh_task,   // Function
        "VFS_Mesh",      // Name
        10000,           // Stack size
        this,            // Parameter
        1,               // Priority
        NULL,            // Handle
        0                // Core 0
    );
}

void VFS::tick() {
    unsigned long now = millis();

    if (has_feature(VFS_HANDSHAKE)) {
        static unsigned long last_discovery_attempt = 0;

        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
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
        std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
        current_peers = peers_;
    }

    for (auto& peer : current_peers) {
        peer->tick();
    }
}

void VFS::trigger_activity() {
    led_state_ = !led_state_; // Toggle state
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, led_state_ ? HIGH : LOW);
#endif
}

void VFS::register_with_neighbors() {
    for (const auto& neighbor : config_.neighbors) {
        Serial.printf("[VFS %s] Registering with neighbor: %s\n", config_.id.c_str(), neighbor.c_str());
        trigger_activity();

        HTTPClient http;
        http.begin((neighbor + "/register").c_str());
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.addHeader("Content-Type", "application/json");
        
        json body = {{"id", config_.id}, {"url", ""}}; 
        int code = http.POST(body.dump().c_str());
        
        if (code == 200) {
            String respText = http.getString();
            json resp = json::parse(respText.c_str());
            std::string neighbor_id = resp.value("id", neighbor);
            
            Serial.printf("[VFS %s] Registered successfully with %s (Neighbor ID: %s, Reachability: %s)\n", 
                          config_.id.c_str(), neighbor.c_str(), neighbor_id.c_str(), resp["reachability"].get<std::string>().c_str());
            
            add_peer(std::make_shared<ReverseConnection>(this, neighbor_id, neighbor));
        } else {
            Serial.printf("[VFS %s] Registration failed for %s: %s (%d)\n", 
                          config_.id.c_str(), neighbor.c_str(), http.errorToString(code).c_str(), code);
        }
        http.end();
    }
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

void VFS::handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond) {
    if (op == "PUB") {
        Serial.printf("[Mesh IN] <- Sovereign Binary PUB: %s (%d bytes)\n", sel.path.c_str(), (int)len);
        trigger_activity();
        // Forward to any local listeners if needed, but for now we just log
    }
}

void VFS::add_peer(std::shared_ptr<Peer> peer) {
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    peers_.push_back(peer);
}

void VFS::clear_peers() {
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    peers_.clear();
}

void VFS::register_op(const std::string& path, Handler handler, const json& schema) {
    handlers_[path] = handler;
    schemas_[path] = schema;
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
