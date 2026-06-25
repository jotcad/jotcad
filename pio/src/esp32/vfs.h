#pragma once

#include "vfs_types.h"
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <sstream>

#ifdef ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

#include <zenoh-pico.h>

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
    bool is_connected() const { return connected_; }

    // Obsolete APIs retained for compatibility
    void register_with_neighbors() {}
    void clear_peers() {}

    using Handler = std::function<void(const json& params, VFSResponseWriter* response)>;
    void register_op(const std::string& path, Handler handler, const json& schema = json::object());

    using NotificationHandler = std::function<void(const Selector& sel, const json& payload)>;
    void on_notification(const std::string& path, NotificationHandler handler);

    json read_selector(const Selector& sel);
    int notify(const Selector& sel, const json& payload, const std::vector<std::string>& stack = {});
    int notify_binary(const Selector& sel, const uint8_t* data, size_t len, const std::vector<std::string>& stack = {});
    void subscribe(const Selector& sel, long long expiresAt, const std::vector<std::string>& stack = {}) {}

    // Internal callback for Zenoh queries
    void handle_query(z_loaned_query_t* query);

private:
    Config config_;
    std::map<std::string, Handler> handlers_;
    std::map<std::string, json> schemas_;

    // Zenoh session and resources
    z_owned_session_t z_session_;
    std::vector<z_owned_queryable_t> z_queryables_;
    std::vector<z_owned_subscriber_t> z_subscribers_;
    bool connected_ = false;

    // Activity LED State
    bool led_state_ = true;
};

} // namespace fs
