#pragma once

#include "../vendor/json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>

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
    void register_op(const std::string& path, OpHandler handler);

    // Starts the HTTP server and joins the mesh.
    void listen();
    
    // Stop the server.
    void stop();

    // Perform a READ (Local Cache -> Provision -> Mesh)
    std::vector<uint8_t> read(const VFSRequest& req);

    // Perform a WRITE (Direct to Local Cache)
    void write(const std::string& path, const json& parameters, const std::vector<uint8_t>& data);

private:
    Config config_;
    std::map<std::string, OpHandler> handlers_;
    void* server_ptr_; // Internal httplib::Server

    std::string get_cid(const std::string& path, const json& parameters);
    bool has_local(const std::string& cid);
    std::vector<uint8_t> get_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
};

} // namespace fs
} // namespace jotcad
