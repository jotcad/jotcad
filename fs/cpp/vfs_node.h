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

    template<typename T = std::vector<uint8_t>>
    T read(const Selector& sel, const std::string& output);

    template<typename T = std::vector<uint8_t>>
    T read(const CID& cid);

    std::vector<uint8_t> spy(const VFSRequest& req);

    template<typename T = std::vector<uint8_t>>
    Selector write(const Selector& sel, const T& data);

    template<typename T = std::vector<uint8_t>>
    Selector write(const Selector& sel, const T& data, const std::string& output);

    template<typename T = std::vector<uint8_t>>
    CID write_anonymous(const T& data);

    // Anonymous write (returns a CID string for backward compatibility until all consumers are updated)
    template<typename T = std::vector<uint8_t>>
    Selector write(const T& data) {
        CID cid = write_anonymous(data);
        return Selector::from_json(json{{"parameters", {{"cid", cid.value}}}});
    }

    Selector write_bytes(const Selector& sel, const std::vector<uint8_t>& data);
    Selector write_bytes(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output);

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

    std::vector<uint8_t> read_impl(const Selector& sel, int depth = 0, std::vector<std::string> stack = {}, const std::string& output = "", long long expiresAt = 0);

    std::string get_cid(const Selector& sel);
    bool has_local(const std::string& cid);
    std::vector<uint8_t> get_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

// --- EXPLICIT SPECIALIZATION DECLARATIONS ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel);
template<> json VFSNode::read<json>(const Selector& sel);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel, const std::string& output);
template<> json VFSNode::read<json>(const Selector& sel, const std::string& output);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid);
template<> json VFSNode::read<json>(const CID& cid);

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data);
template<> Selector VFSNode::write<json>(const Selector& sel, const json& data);
template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data);

template<> Selector VFSNode::write<std::vector<uint8_t>>(const Selector& sel, const std::vector<uint8_t>& data, const std::string& output);
template<> Selector VFSNode::write<json>(const Selector& sel, const json& data, const std::string& output);
template<> Selector VFSNode::write<std::string>(const Selector& sel, const std::string& data, const std::string& output);

template<> CID VFSNode::write_anonymous<std::vector<uint8_t>>(const std::vector<uint8_t>& data);
template<> CID VFSNode::write_anonymous<json>(const json& data);
template<> CID VFSNode::write_anonymous<std::string>(const std::string& data);

} // namespace fs
