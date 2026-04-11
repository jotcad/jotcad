#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include "../vendor/json.hpp"

namespace jotcad {
namespace fs {

using json = nlohmann::json;

/**
 * Event representing a state change on the blackboard.
 */
struct VFSEvent {
    std::string path;
    json parameters;
    std::string state;
    std::string source;
};

/**
 * Callback function for watches.
 */
using WatchCallback = std::function<void(const VFSEvent&)>;

/**
 * Interface for a Symmetric VFS Peer in C++.
 */
class VFSClient {
public:
    virtual ~VFSClient() = default;

    // --- Blackboard API (Blocking) ---

    /**
     * Synchronously read an artifact. Blocks until AVAILABLE.
     */
    virtual std::vector<uint8_t> read(const std::string& path, const json& parameters = json::object(), const std::vector<std::string>& stack = {}) = 0;

    /**
     * Synchronously write an artifact.
     */
    virtual void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data) = 0;

    /**
     * Create an alias from source to target.
     */
    virtual void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) = 0;

    /**
     * Attempt to secure a time-limited lease.
     */
    virtual bool lease(const std::string& path, const json& parameters, int duration_ms) = 0;

    /**
     * Check the current state of a coordinate.
     */
    virtual std::string status(const std::string& path, const json& parameters = json::object()) = 0;

    /**
     * Manually set the state of a coordinate.
     */
    virtual void write_state(const std::string& path, const json& parameters, const std::string& state, const std::string& source = "") = 0;

    /**
     * Declare a JSON schema for a path.
     */
    virtual void declare(const std::string& path, const json& schema) = 0;

    /**
     * Manifest demand for a coordinate.
     */
    virtual std::string tickle(const std::string& path, const json& parameters = json::object()) = 0;

    /**
     * Calculate the Content-ID for a path and parameters.
     */
    virtual std::string get_cid(const std::string& path, const json& parameters = json::object()) = 0;

    // --- Observation API ---

    /**
     * Start watching a region of the blackboard. 
     * Runs the callback on a background thread or via manual polling.
     */
    virtual void watch(const std::string& path_pattern, WatchCallback callback) = 0;

    // --- Peer Coordination ---

    /**
     * Set a handler for inbound READ requests (Virtual Mailbox support).
     */
    virtual void on_read(std::function<std::vector<uint8_t>(const std::string&, const json&)> handler) = 0;
};

/**
 * Factory to create a REST-based VFS client.
 */
std::unique_ptr<VFSClient> create_rest_client(const std::string& base_url, const std::string& peer_id);

} // namespace fs
} // namespace jotcad
