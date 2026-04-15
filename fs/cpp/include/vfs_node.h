#pragma once

#include "../vendor/json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <mutex>

namespace httplib {
    class Response;
}

namespace jotcad {
namespace fs {

using json = nlohmann::json;

/**
 * VFSNode: A decentralized mesh participant that can provision files on-demand.
 * 100% C++ Native, 100% Demand-Driven.
 */
class VFSNode {
public:
    struct Config {
        std::string id;
        std::string version;
        std::string storage_dir;
        std::vector<std::string> neighbors;
        int port = 9090;
    };

    struct VFSRequest {
        std::string path;
        json parameters;
        std::vector<std::string> stack;
        long long expiresAt = 0;
    };

    // An OpHandler takes a full request context and returns bytes.
    using OpHandler = std::function<std::vector<uint8_t>(const VFSRequest& req)>;

    VFSNode(const Config& config);
    ~VFSNode();

    // Register a local "Provisioning Op" (e.g., "shape/box")
    void register_op(const std::string& path, OpHandler handler, const json& schema = json::object());

    // Starts the HTTP server and joins the mesh.
    void listen();
    
    // Stop the server.
    void stop();

    // Perform a READ (Local Cache -> Provision -> Mesh)
    std::vector<uint8_t> read(const VFSRequest& req);

    // Perform a SPY (Local Discovery -> Mesh Discovery)
    std::vector<uint8_t> spy(const VFSRequest& req);

    // Perform a WRITE (Direct to Local Cache)
    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data);

    // Perform a Content-Addressable WRITE (Returns CID)
    std::string write_cid(const std::string& path, const std::vector<uint8_t>& data);

    // Create a LINK (alias) between two coordinates
    void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);

    // Validate a request against its schema
    bool validate_selector(const VFSRequest& req, std::string& error_out);

    // Pub-Sub
    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack);
    void notify(const json& selector, const json& payload, const std::vector<std::string>& stack = {});

    // Add a peer via handshake
    void add_peer(const std::string& url);

    // Register a reverse peer connection (for incoming commands)
    void register_reverse_peer(const std::string& peer_id, httplib::Response& res);

private:
    Config config_;
    std::map<std::string, OpHandler> handlers_;
    std::map<std::string, json> schemas_;
    void* server_ptr_; // Internal httplib::Server
    
    // Peer management
    std::map<std::string, std::string> peers_; // ID -> URL
    std::set<std::string> connecting_;
    std::mutex peer_mutex_;

    // Interest Table: Serialized Selector -> Map<NeighborID, Expiry>
    std::map<std::string, std::map<std::string, long long>> interests_;
    std::mutex interest_mutex_;
    
    // We also need to store the actual json selector for each key to support semantic matching
    std::map<std::string, json> interest_selectors_;

    // Reverse connection registry
    std::map<std::string, httplib::Response*> reverse_listeners_;
    std::mutex reverse_mutex_;

    std::string get_cid(const std::string& path, const json& parameters);
    bool has_local(const std::string& cid);
    std::vector<uint8_t> get_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

} // namespace fs
} // namespace jotcad
