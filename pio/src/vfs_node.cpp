#include "vfs_node.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <set>

namespace fs {

/**
 * VFSRequest Implementation for Reverse Connection
 */
class ReverseRequest : public VFSRequest {
public:
    ReverseRequest(std::function<void(int, const char*, const char*)> respond) : respond_(respond) {}
    void send(int code, const char* contentType, const char* body) override {
        respond_(code, contentType, body);
    }
private:
    std::function<void(int, const char*, const char*)> respond_;
};

/**
 * ReverseConnection Implementation
 */
ReverseConnection::ReverseConnection(VFSNode* node, const std::string& neighbor_id, const std::string& url) 
    : node_(node), id_(neighbor_id), url_(url) {
    if (url_.back() == '/') url_.pop_back();
}

void ReverseConnection::notify(const json& selector, const json& payload, const std::string& source_id) {
    std::string path = selector.value("path", "unknown");
    Serial.printf("[Mesh OUT] -> NOTIFY: %s to %s\n", path.c_str(), id_.c_str());
    node_->trigger_activity();

    HTTPClient http;
    http.begin((url_ + "/notify").c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-VFS-Id", source_id.c_str());

    json body = {
        {"selector", selector},
        {"payload", payload},
        {"stack", {source_id}}
    };

    int code = http.POST(body.dump().c_str());
    if (code != 200) {
        Serial.printf("[ReverseConn %s] Notify failed: %d\n", id_.c_str(), code);
    }
    http.end();
}

void ReverseConnection::loop() {
    unsigned long now = millis();
    static unsigned long poll_interval = 100;
    if (now - last_poll_ < poll_interval && last_poll_ != 0) return;

    HTTPClient http;
    http.begin((url_ + "/listen").c_str());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("x-vfs-peer-id", node_->id().c_str());
    http.setTimeout(60000); // 60 Seconds (Max uint16_t is ~65s)
    
    int code;
    if (has_reply_) {
        Serial.printf("[Mesh OUT] -> REPLY to %s (%d bytes)\n", reply_to_.c_str(), (int)reply_body_.length());
        node_->trigger_activity();
        http.addHeader("x-vfs-reply-to", reply_to_.c_str());
        http.addHeader("Content-Type", reply_content_type_.c_str());
        code = http.POST(reply_body_.c_str());
        has_reply_ = false;
        reply_to_ = "";
        reply_body_ = "";
    } else {
        code = http.POST("");
    }

    last_poll_ = millis();

    if (code == 200) {
        String payload = http.getString();
        if (payload.length() > 0) {
            node_->trigger_activity();
            try {
                json cmd = json::parse(payload.c_str());
                std::string type = cmd.value("type", "unknown");
                std::string op = cmd.value("op", "none");
                Serial.printf("[Mesh IN] <- %s/%s from %s\n", type.c_str(), op.c_str(), id_.c_str());
                
                if (cmd.contains("id")) reply_to_ = cmd["id"];
                
                node_->handle_command(cmd, [this](int code, const char* type, const char* body) {
                    this->reply_body_ = body;
                    this->reply_content_type_ = type;
                    this->has_reply_ = true;
                });
                poll_interval = 50;
            } catch (...) {
                Serial.printf("[ReverseConn %s] JSON Parse Error in command\n", id_.c_str());
            }
        } else {
            poll_interval = 1000;
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
 * VFSNode Implementation
 */
VFSNode::VFSNode(const Config& config) : config_(config) {
}

VFSNode::~VFSNode() {
}

// Background Task for Mesh Listeners
void vfs_mesh_task(void* parameter) {
    VFSNode* node = (VFSNode*)parameter;
    Serial.println("\n[VFS] Mesh Task Started on Core 0");
    Serial.flush();
    while (true) {
        node->loop();
        vTaskDelay(pdMS_TO_TICKS(10)); // Yield
    }
}

void VFSNode::begin() {
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

void VFSNode::loop() {
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
        peer->loop();
    }
}

void VFSNode::trigger_activity() {
    led_state_ = !led_state_; // Toggle state
#ifdef LED_BUILTIN
    digitalWrite(LED_BUILTIN, led_state_ ? HIGH : LOW);
#endif
}

void VFSNode::register_with_neighbors() {
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

void VFSNode::handle_command(const json& cmd, std::function<void(int, const char*, const char*)> respond) {
    if (!cmd.contains("type")) return;
    std::string type = cmd.at("type");
    
    if (type == "COMMAND") {
        std::string op = cmd.at("op");
        if (op == "READ" && has_feature(VFS_FULFILLMENT)) {
            Selector sel = Selector::from_json(cmd.at("selector"));
            Serial.printf("[VFS] Processing READ: %s\n", sel.path.c_str());
            trigger_activity();
            if (handlers_.count(sel.path)) {
                ReverseRequest req(respond);
                handlers_[sel.path](sel.parameters, &req);
            } else {
                respond(404, "text/plain", "Not Found");
            }
        } else if (op == "SUB" && has_feature(VFS_SUBSCRIPTION)) {
            Selector sel = Selector::from_json(cmd.at("selector"));
            long long expiresAt = cmd.at("expiresAt");
            std::string neighbor_id = cmd.at("stack").back();
            Serial.printf("[VFS] Processing SUB: %s from %s\n", sel.path.c_str(), neighbor_id.c_str());
            trigger_activity();
            this->subscribe(sel.to_json(), expiresAt, neighbor_id);
        }
    } else if (type == "NOTIFY") {
        Selector sel = Selector::from_json(cmd.at("selector"));
        Serial.printf("[Mesh IN] <- NOTIFY: %s\n", sel.path.c_str());
        trigger_activity();
    }
}

void VFSNode::add_peer(std::shared_ptr<Peer> peer) {
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    peers_.push_back(peer);
}

void VFSNode::clear_peers() {
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    peers_.clear();
}

void VFSNode::register_op(const std::string& path, Handler handler, const json& schema) {
    handlers_[path] = handler;
    schemas_[path] = schema;
}

int VFSNode::notify(const json& selector, const json& payload) {
    if (!has_feature(VFS_PUBLICATION)) return 0;

    std::string path = selector.at("path");
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

    for (const auto& target_id : targets) {
        for (auto& peer : current_peers) {
            if (peer->id() == target_id) {
                trigger_activity();
                peer->notify(selector, payload, config_.id);
                notified++;
            }
        }
    }
    return notified;
}

void VFSNode::subscribe(const json& selector, long long expiresAt, const std::string& remote_id) {
    if (!has_feature(VFS_SUBSCRIPTION)) return;
    std::string path = selector.at("path");
    std::lock_guard<std::recursive_mutex> lock(mesh_mutex_);
    interests_[path][remote_id] = expiresAt;
    interest_selectors_[path] = selector;
    Serial.printf("[VFS %s] Subscription recorded for %s from %s (expires in %lldms)\n", 
                  config_.id.c_str(), path.c_str(), remote_id.c_str(), expiresAt - millis());
}

} // namespace fs
