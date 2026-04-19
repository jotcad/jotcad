#pragma once

#include "../vendor/json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <stdexcept>

namespace httplib {
    class Response;
}

namespace jotcad {
namespace fs {

using json = nlohmann::json;

// Global VFS utility declarations
std::string vfs_hash256(const std::vector<uint8_t>& data);
std::string vfs_hash256_str(const std::string& data);

/**
 * Selector: The universal address for mesh content.
 */
struct Selector {
    std::string path;
    json parameters = json::object();

    json to_json() const { return {{"path", path}, {"parameters", parameters}}; }
    static Selector from_json(const json& j) {
        if (j.is_string()) return {j.get<std::string>(), json::object()};
        return {j.at("path").get<std::string>(), j.value("parameters", json::object())};
    }
};

/**
 * VFSException: Signals resolution or validation failures within the mesh.
 */
class VFSException : public std::runtime_error {
public:
    int code;
    VFSException(const std::string& msg, int code = 500) : std::runtime_error(msg), code(code) {}
};

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

    using OpHandler = std::function<std::vector<uint8_t>(const VFSRequest& req)>;

    VFSNode(const Config& config);
    ~VFSNode();

    // Register a local "Provisioning Op" (e.g., "shape/box")
    void register_op(const std::string& path, OpHandler handler, const json& schema = json::object());

    // Broadcast the current schema catalog to the mesh
    void notify_schema();

    // Starts the HTTP server and joins the mesh.
    void listen();
    void stop();

    // Perform a READ (Local Cache -> Provision -> Mesh)
    template<typename T = std::vector<uint8_t>>
    T read(const VFSRequest& req);

    // Perform a SPY (Local Discovery -> Mesh Discovery)
    std::vector<uint8_t> spy(const VFSRequest& req);

    // Perform a WRITE (Direct to Local Cache)
    template<typename T = std::vector<uint8_t>>
    void write(const std::string& path, const json& parameters, const T& data) {
        std::vector<uint8_t> bytes;
        if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            bytes = data;
        } else if constexpr (std::is_same_v<T, json>) {
            std::string s = data.dump();
            bytes = std::vector<uint8_t>(s.begin(), s.end());
        } else if constexpr (std::is_same_v<T, std::string>) {
            bytes = std::vector<uint8_t>(data.begin(), data.end());
        } else {
            // Fallback for types specialized in other headers (e.g. Shape/Geometry)
            write_specialized(path, parameters, data);
            return;
        }
        write_local(get_cid(path, parameters), bytes, path, parameters);
    }

    template<typename T = std::vector<uint8_t>>
    std::string write_with_cid(const std::string& path, const T& data) {
        std::vector<uint8_t> bytes;
        if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
            bytes = data;
        } else if constexpr (std::is_same_v<T, json>) {
            std::string s = data.dump();
            bytes = std::vector<uint8_t>(s.begin(), s.end());
        } else if constexpr (std::is_same_v<T, std::string>) {
            bytes = std::vector<uint8_t>(data.begin(), data.end());
        } else {
            return write_with_cid_specialized(path, data);
        }
        
        std::string hash = vfs_hash256(bytes);
        json parameters = {{"cid", hash}};
        write_local(get_cid(path, parameters), bytes, path, parameters);
        return hash;
    }

    template<typename T = std::vector<uint8_t>>
    Selector write_geometry(const T& data) {
        return {"geo/mesh", {{"cid", write_with_cid("geo/mesh", data)}}};
    }

    template<typename T = std::vector<uint8_t>>
    Selector write_shape(const T& data) {
        return {"geo/mesh", {{"cid", write_with_cid("geo/mesh", data)}}};
    }

    template<typename T> void write_specialized(const std::string&, const json&, const T&);
    template<typename T> std::string write_with_cid_specialized(const std::string&, const T&);

    void link(const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);

    bool validate_selector(const VFSRequest& req, std::string& error_out);

    // Pub-Sub
    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack);
    void notify(const json& selector, const json& payload, const std::vector<std::string>& stack = {});

    void add_peer(const std::string& url);
    void register_reverse_peer(const std::string& peer_id, httplib::Response& res);

    // Accessors for state-push
    json get_catalog();
    json get_neighbors();

private:
    Config config_;
    std::map<std::string, OpHandler> handlers_;
    std::map<std::string, json> schemas_;
    void* server_ptr_; 
    
    std::map<std::string, std::string> peers_; // ID -> URL
    std::set<std::string> connecting_;
    std::mutex peer_mutex_;

    std::map<std::string, std::map<std::string, long long>> interests_;
    std::mutex interest_mutex_;
    std::map<std::string, json> interest_selectors_;

    std::mutex handlers_mutex_;
    std::mutex storage_mutex_;

    std::vector<uint8_t> read_impl(const VFSRequest& req, int depth = 0);

    std::string get_cid(const std::string& path, const json& parameters);
    bool has_local(const std::string& cid);
    std::vector<uint8_t> get_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

template<>
inline std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req) {
    return read_impl(req);
}

template<>
inline json VFSNode::read<json>(const VFSRequest& req) {
    auto bytes = read_impl(req);
    try {
        return json::parse(bytes);
    } catch (const std::exception& e) {
        throw VFSException(std::string("JSON Parse Error: ") + e.what(), 400);
    }
}

} // namespace fs
} // namespace jotcad
