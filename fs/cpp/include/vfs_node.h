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
namespace geo {
    struct Geometry;
    struct Shape;
}
}

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
        Selector s;
        if (j.contains("path")) s.path = j.at("path").get<std::string>();
        if (j.contains("parameters") && j.at("parameters").is_object()) s.parameters = j.at("parameters");
        return s;
    }
};

// nlohmann::json integration for Selector
inline void to_json(json& j, const Selector& s) { j = s.to_json(); }
inline void from_json(const json& j, Selector& s) {
    if (j.is_string()) { s.path = j.get<std::string>(); s.parameters = json::object(); }
    else {
        if (j.contains("path")) s.path = j.at("path").get<std::string>();
        if (j.contains("parameters") && j.at("parameters").is_object()) s.parameters = j.at("parameters");
        else s.parameters = json::object();
    }
}

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
        Selector selector;
        std::vector<std::string> stack;
        long long expiresAt = 0;
    };

    using OpHandler = std::function<std::vector<uint8_t>(const VFSRequest& req)>;

    VFSNode(const Config& config);
    ~VFSNode();

    void register_op(const std::string& path, OpHandler handler, const json& schema = json::object());
    void notify_schema();
    void listen();
    void stop();

    template<typename T = std::vector<uint8_t>>
    T read(const Selector& sel);

    std::vector<uint8_t> spy(const VFSRequest& req);

    template<typename T = std::vector<uint8_t>>
    Selector write(const Selector& sel, const T& data);

    // Anonymous write (only for types that support content-addressing like Geometry)
    template<typename T = std::vector<uint8_t>>
    Selector write(const T& data) {
        return write(Selector{}, data);
    }

    Selector write_bytes(const Selector& sel, const std::vector<uint8_t>& data);

    void link(const Selector& src, const Selector& tgt);

    bool validate_selector(const VFSRequest& req, std::string& error_out);

    void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack);
    void notify(const json& selector, const json& payload, const std::vector<std::string>& stack = {});

    void add_peer(const std::string& url);
    void register_reverse_peer(const std::string& peer_id, httplib::Response& res);

    json get_catalog();
    json get_neighbors();

private:
    Config config_;
    std::map<std::string, OpHandler> handlers_;
    std::map<std::string, json> schemas_;
    void* server_ptr_; 
    
    std::map<std::string, std::string> peers_; 
    std::set<std::string> connecting_;
    std::mutex peer_mutex_;

    std::map<std::string, std::map<std::string, long long>> interests_;
    std::mutex interest_mutex_;
    std::map<std::string, json> interest_selectors_;

    std::mutex handlers_mutex_;
    std::mutex storage_mutex_;

    std::vector<uint8_t> read_impl(const Selector& sel, int depth = 0);

    std::string get_cid(const Selector& sel);
    bool has_local(const std::string& cid);
    std::vector<uint8_t> get_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

// --- CORE SPECIALIZATION DECLARATIONS ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel);
template<> json VFSNode::read<json>(const Selector& sel);
template<> jotcad::geo::Geometry VFSNode::read<jotcad::geo::Geometry>(const Selector& sel);
template<> jotcad::geo::Shape VFSNode::read<jotcad::geo::Shape>(const Selector& sel);

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data);
template<> Selector VFSNode::write<json>(const Selector& sel, const json& data);
template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data);
template<> Selector VFSNode::write<jotcad::geo::Geometry>(const Selector& sel, const jotcad::geo::Geometry& data);
template<> Selector VFSNode::write<jotcad::geo::Shape>(const Selector& sel, const jotcad::geo::Shape& data);

} // namespace fs
