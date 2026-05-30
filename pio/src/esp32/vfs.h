#pragma once

#include "vfs_types.h"
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <WebSocketsClient.h>

namespace fs {

enum VFSFeature {
    VFS_NONE         = 0,
    VFS_HANDSHAKE    = 1 << 0, // Mandatory for mesh participation
    VFS_FULFILLMENT  = 1 << 1, // Respond to /read
    VFS_PUBLICATION  = 1 << 2, // Emit /publish
    VFS_SUBSCRIPTION = 1 << 3, // Respond to /subscribe
    VFS_STORAGE      = 1 << 4, // Persistent storage (Flash)
    VFS_ALL          = 0xFF
};

/**
 * VFSResponseWriter: Protocol-agnostic response handle.
 */
class VFSResponseWriter {
public:
    virtual ~VFSResponseWriter() = default;
    virtual void send(int code, const char* contentType, const char* body) = 0;
    virtual void send_binary(int code, const char* contentType, const uint8_t* data, size_t len) = 0;
};

struct VFSRequest {
    std::string op; // "PUB", "SUB", "READ_SELECTOR", "READ_CID"
    std::string txId;
    Selector selector;
    std::string cid;
    json payload;
    const uint8_t* binary_payload = nullptr;
    size_t binary_len = 0;
    std::vector<std::string> stack;
    std::vector<std::string> resolutionStack;
    long long expiresAt = 0;
};

/**
 * Peer: Abstract mesh neighbor.
 */
class Peer {
public:
    virtual ~Peer() = default;
    virtual const std::string& id() const = 0;
    virtual void send(const VFSRequest& req) = 0;
    virtual void tick() {} // No-op for forward-only peers
    virtual std::string protocol() const = 0;
    virtual std::string reachability() const = 0;
};

class VFS;

/**
 * ReverseConnection: Implements the JotCAD /listen polling pattern.
 */
class ReverseConnection : public Peer {
public:
    ReverseConnection(VFS* node, const std::string& neighbor_id, const std::string& url);
    const std::string& id() const override { return id_; }
    void send(const VFSRequest& req) override;
    
    void tick() override; 
    std::string protocol() const override { return "REVERSE-HTTP"; }
    std::string reachability() const override { return "REVERSE"; }
private:
    VFS* node_;
    std::string id_;
    std::string url_;
    
    // Reply buffer for the NEXT /listen call
    std::string reply_to_;
    std::vector<uint8_t> reply_body_;
    std::string reply_content_type_;
    std::string reply_encoding_;
    std::string reply_selector_b64_;
    bool has_reply_ = false;
    
    unsigned long last_poll_ = 0;
};

/**
 * WSConnection: Implements the JotCAD persistent WebSocket transport.
 */
class WSConnection : public Peer {
public:
    WSConnection(VFS* node, const std::string& neighbor_id, const std::string& url);
    ~WSConnection() override;
    
    const std::string& id() const override { return id_; }
    void send(const VFSRequest& req) override;
    void tick() override;
    std::string protocol() const override { return "WS"; }
    std::string reachability() const override { return "DIRECT"; }

private:
    void on_event(WStype_t type, uint8_t * payload, size_t length);

    VFS* node_;
    std::string id_;
    std::string url_;
    WebSocketsClient client_;
    
    bool connected_ = false;
    json expecting_binary_header_;
};

class VFS {
public:
    struct Config {
        std::string id;
        uint32_t enabled_features = VFS_HANDSHAKE;
        std::vector<std::string> neighbors;
    };

    VFS(const Config& config);
    ~VFS();

    void begin();
    void tick();
    
    // Activity UI (Toggle-based)
    void trigger_activity();

    const std::string& id() const { return config_.id; }
    bool has_feature(VFSFeature feature) const { return (config_.enabled_features & feature) != 0; }

    void register_with_neighbors();
    void add_peer(std::shared_ptr<Peer> peer);
    void upgrade_peer_to_ws(const std::string& peer_id, std::shared_ptr<Peer> ws_conn);
    void clear_peers();

    using Handler = std::function<void(const json& params, VFSResponseWriter* response)>;
    void register_op(const std::string& path, Handler handler, const json& schema = json::object());

    int notify(const json& selector, const json& payload, const std::vector<std::string>& stack = {});
    int notify_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack = {});
    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack = {});

    
    // Internal API for Peer loop interaction
    void handle_command(const json& cmd, std::function<void(int, const char*, const uint8_t*, size_t)> respond);
    void handle_ws_frame(const json& frame, WSConnection* conn);
    void handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond);

private:
    Config config_;
    std::map<std::string, Handler> handlers_;
    std::map<std::string, json> schemas_;

    // Peer Management
    std::vector<std::shared_ptr<Peer>> peers_;
#ifdef ESP32
    std::recursive_mutex mesh_mutex_;
#else
    Mutex mesh_mutex_;
#endif

    // Interest Tracking
    std::map<std::string, std::map<std::string, long long>> interests_;
    std::map<std::string, json> interest_selectors_;

    // Activity LED State
    bool led_state_ = true;
};

} // namespace fs
