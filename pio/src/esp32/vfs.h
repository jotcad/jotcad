#pragma once

#include "vfs_types.h"
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>

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
 * VFSRequest: Protocol-agnostic response handle.
 */
class VFSRequest {
public:
    virtual ~VFSRequest() = default;
    virtual void send(int code, const char* contentType, const char* body) = 0;
    virtual void send_binary(int code, const char* contentType, const uint8_t* data, size_t len) = 0;
};

/**
 * Peer: Abstract mesh neighbor.
 */
class Peer {
public:
    virtual ~Peer() = default;
    virtual const std::string& id() const = 0;
    virtual void publish(const json& selector, const json& payload, const std::vector<std::string>& stack) = 0;
    virtual void publish_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack) = 0;
    virtual void tick() {} // No-op for forward-only peers
};

class VFS;

/**
 * ReverseConnection: Implements the JotCAD /listen polling pattern.
 */
class ReverseConnection : public Peer {
public:
    ReverseConnection(VFS* node, const std::string& neighbor_id, const std::string& url);
    const std::string& id() const override { return id_; }
    void publish(const json& selector, const json& payload, const std::vector<std::string>& stack) override;
    void publish_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack) override;
    
    void tick() override; 
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
    void clear_peers();

    using Handler = std::function<void(const json& params, VFSRequest* request)>;
    void register_op(const std::string& path, Handler handler, const json& schema = json::object());

    int publish(const json& selector, const json& payload, const std::vector<std::string>& stack = {});
    int publish_binary(const json& selector, const uint8_t* data, size_t len, const std::vector<std::string>& stack = {});
    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack = {});
    
    // Internal API for Peer loop interaction
    void handle_command(const json& cmd, std::function<void(int, const char*, const uint8_t*, size_t)> respond);
    void handle_binary_command(const std::string& op, const Selector& sel, const uint8_t* data, size_t len, const std::string& replyTo, std::function<void(int, const char*, const uint8_t*, size_t)> respond);

private:
    Config config_;
    std::map<std::string, Handler> handlers_;
    std::map<std::string, json> schemas_;
    std::vector<std::shared_ptr<Peer>> peers_;
    std::recursive_mutex mesh_mutex_;

    // Interest Tracking
    std::map<std::string, std::map<std::string, long long>> interests_;
    std::map<std::string, json> interest_selectors_;

    // Activity LED State
    bool led_state_ = true;
};

} // namespace fs
