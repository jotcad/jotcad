#pragma once

#include "vendor/json.hpp"
#include "selector.h"
#include "cid_type.h"
#include "vfs_exception.h"
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
namespace geo {
    struct Geometry;
    struct Shape;
}
}

namespace fs {

using json = nlohmann::json;

// Global VFS utility declarations (from cid.h)
std::string vfs_hash256(const std::vector<uint8_t>& data);
std::string vfs_hash256_str(const std::string& data);

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
        std::string cid;   // Direct content identity
        Selector selector; // Computational identity
        std::vector<std::string> stack;
        long long expiresAt = 0;
        bool followLinks = true;

        bool is_cid() const { return !cid.empty(); }
    };

    using OpHandler = std::function<void(const VFSRequest& req)>;

    VFSNode(const Config& config);
    ~VFSNode();

    void register_op(const std::string& path, OpHandler handler, const json& schema = json::object());
    void notify_schema();
    void listen();
    void stop();

    template<typename T = std::vector<uint8_t>>
    T read(const Selector& sel);

    template<typename T = std::vector<uint8_t>>
    T read(const VFSRequest& req);

    template<typename T = std::vector<uint8_t>>
    T read(const CID& cid);

    std::string get_cid(const Selector& sel);
    std::vector<uint8_t> get_local(const std::string& cid);
    std::vector<uint8_t> spy(const VFSRequest& req);

    Selector write(const Selector& sel, const json& data);
    Selector write(const Selector& sel, const std::vector<uint8_t>& data);
    Selector write(const Selector& sel, const std::string& data);
    Selector write(const Selector& sel, const jotcad::geo::Geometry& data);
    Selector write(const Selector& sel, const jotcad::geo::Shape& data);

    template<typename T>
    CID materialize(const T& data);

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

    std::map<std::string, std::vector<json>> reverse_queues_;
    std::map<std::string, httplib::Response*> reverse_listeners_;
    std::mutex reverse_mutex_;

    std::vector<uint8_t> read_impl(const VFSRequest& req);

    bool has_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

// --- EXPLICIT SPECIALIZATION DECLARATIONS ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel);
template<> json VFSNode::read<json>(const Selector& sel);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req);
template<> json VFSNode::read<json>(const VFSRequest& req);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid);
template<> json VFSNode::read<json>(const CID& cid);

template<> CID VFSNode::materialize<std::vector<uint8_t>>(const std::vector<uint8_t>& data);
template<> CID VFSNode::materialize<json>(const json& data);
template<> CID VFSNode::materialize<std::string>(const std::string& data);
template<> CID VFSNode::materialize<jotcad::geo::Shape>(const jotcad::geo::Shape& data);

} // namespace fs
