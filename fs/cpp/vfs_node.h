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
#include <condition_variable>

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
struct VFSResult {
    std::vector<uint8_t> data;
    json metadata;
};

class VFSNode {
public:
    struct Config {
        std::string id;
        std::string version;
        std::string storage_dir;
        std::vector<std::string> neighbors;
        std::string cert_path;
        std::string key_path;
        int port = 9090;
    };

    struct VFSRequest {
        std::string cid;   // Direct content identity
        Selector selector; // Computational identity
        std::vector<std::string> stack;
        std::vector<std::string> resolutionStack;
        long long expiresAt = 0;
        bool followLinks = true;

        bool is_cid() const { return !cid.empty(); }
    };

    // Mesh Connection Abstraction
    struct Connection {
        std::string neighbor_id;
        virtual ~Connection() = default;
        virtual void notify(const json& selector, const json& payload, const std::vector<std::string>& stack) = 0;
        virtual void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) = 0;
        virtual VFSResult read(const VFSRequest& req) = 0;
        virtual bool is_reverse() const = 0;
        virtual std::string get_url() const { return ""; }
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
    VFSResult get_local(const std::string& cid);

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
    void add_connection(std::shared_ptr<Connection> conn);
    void register_reverse_peer(const std::string& peer_id, httplib::Response& res);

    json get_catalog();
    json get_neighbors();

private:
    struct ForwardConnection : public Connection {
        std::string url;
        ForwardConnection(std::string id, std::string u) { neighbor_id = std::move(id); url = std::move(u); }
        void notify(const json& selector, const json& payload, const std::vector<std::string>& stack) override;
        void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) override;
        VFSResult read(const VFSRequest& req) override;
        bool is_reverse() const override { return false; }
        std::string get_url() const override { return url; }
    };

    struct ReverseConnection : public Connection {
        std::vector<json> queue;
        std::mutex mutex;
        std::condition_variable cv;
        bool is_polling = false;
        ReverseConnection(std::string id) { neighbor_id = std::move(id); }
        void notify(const json& selector, const json& payload, const std::vector<std::string>& stack) override;
        void subscribe(const json& selector, long long expiresAt, const std::vector<std::string>& stack) override;
        VFSResult read(const VFSRequest& req) override;
        bool is_reverse() const override { return true; }
    };

    Config config_;
    std::map<std::string, OpHandler> handlers_;
    std::map<std::string, json> schemas_;
    void* server_ptr_; 
    
    std::map<std::string, std::shared_ptr<Connection>> peers_; 
    std::set<std::string> connecting_;
    std::mutex peer_mutex_;

    std::map<std::string, std::map<std::string, long long>> interests_;
    std::mutex interest_mutex_;
    std::map<std::string, json> interest_selectors_;

    std::mutex handlers_mutex_;
    std::mutex storage_mutex_;

    VFSResult read_impl(const VFSRequest& req);
    VFSResult read_result(const VFSRequest& req);

    bool has_local(const std::string& cid);
    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);
};

// --- EXPLICIT SPECIALIZATION DECLARATIONS ---

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel);
template<> json VFSNode::read<json>(const Selector& sel);
template<> double VFSNode::read<double>(const Selector& sel);
template<> int VFSNode::read<int>(const Selector& sel);
template<> std::string VFSNode::read<std::string>(const Selector& sel);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req);
template<> json VFSNode::read<json>(const VFSRequest& req);
template<> double VFSNode::read<double>(const VFSRequest& req);
template<> int VFSNode::read<int>(const VFSRequest& req);
template<> std::string VFSNode::read<std::string>(const VFSRequest& req);
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid);
template<> json VFSNode::read<json>(const CID& cid);
template<> double VFSNode::read<double>(const CID& cid);
template<> int VFSNode::read<int>(const CID& cid);
template<> std::string VFSNode::read<std::string>(const CID& cid);

template<> CID VFSNode::materialize<std::vector<uint8_t>>(const std::vector<uint8_t>& data);
template<> CID VFSNode::materialize<json>(const json& data);
template<> CID VFSNode::materialize<std::string>(const std::string& data);
template<> CID VFSNode::materialize<jotcad::geo::Shape>(const jotcad::geo::Shape& data);

} // namespace fs
