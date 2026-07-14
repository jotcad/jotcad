#pragma once

#include "vendor/json.hpp"
#include "selector.h"
#include "cid_type.h"
#include "vfs_exception.h"
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>

namespace jotcad {
namespace geo {
    struct Geometry;
    struct Shape;
}
}

namespace fs {

using json = nlohmann::json;

std::string vfs_hash256(const std::vector<uint8_t>& data);
std::string vfs_hash256_str(const std::string& data);

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
        int max_concurrent_ops = 4;

        static Config load_from_env();
    };

    struct VFSRequest {
        std::string op;
        std::string cid;
        Selector selector;
        std::string data;
        std::vector<uint8_t> binary_data;
        std::vector<std::string> stack;
        std::vector<std::string> resolutionStack;
        long long expiresAt = 0;
        bool followLinks = true;
        bool localOnly = false;

        bool is_cid() const { return !cid.empty(); }
    };

    using OpHandler = std::function<void(const VFSRequest& req)>;

    VFSNode(const Config& config);
    ~VFSNode();

    void register_op(const std::string& path, OpHandler handler, const json& schema = json::object());
    void notify_schema() {}
    void listen();
    void stop();

    // Identity Duality Overloads (Explicit Typed Dispatch)
    template<typename T = std::vector<uint8_t>>
    T read(const Selector& sel);

    template<typename T = std::vector<uint8_t>>
    T read(const CID& cid);

    template<typename T = std::vector<uint8_t>>
    T read(const VFSRequest& req);

    // Internal explicit methods
    template<typename T = std::vector<uint8_t>>
    T readSelector(const Selector& sel) { return read<T>(sel); }

    template<typename T = std::vector<uint8_t>>
    T readCID(const CID& cid) { return read<T>(cid); }

    std::string get_cid(const Selector& sel);
    VFSResult get_local(const std::string& cid);
    bool has_local(const std::string& cid);

    Selector write(const Selector& sel, const json& data);
    Selector write(const Selector& sel, const std::vector<uint8_t>& data);
    Selector write(const Selector& sel, const std::string& data);
    Selector write(const Selector& sel, const jotcad::geo::Geometry& data);
    Selector write(const Selector& sel, const jotcad::geo::Shape& data);

    template<typename T>
    CID materialize(const T& data);

    Selector write_bytes(const Selector& sel, const std::vector<uint8_t>& data);

    void link(const Selector& src, const Selector& tgt);

    void subscribe(const Selector& selector, long long expiresAt, const std::vector<std::string>& stack) {}
    void notify(const Selector& selector, const json& payload, const std::vector<std::string>& stack = {});

    void publish(const std::string& path, const json& payload);
    void publish_binary(const std::string& path, const uint8_t* data, size_t len);

    using SubscriptionCallback = std::function<void(const json& value)>;
    void subscribe(const std::string& path, SubscriptionCallback callback);

    // Path-oriented API
    void write(const std::string& path, const json& value) { publish(path, value); }
    void write_binary(const std::string& path, const uint8_t* data, size_t len) { publish_binary(path, data, len); }
    void listen(const std::string& path, SubscriptionCallback callback) { subscribe(path, callback); }
    void unlisten(const std::string& path);
    json read(const std::string& path);

    void write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params);
    void write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params);

    json get_catalog();
    json get_neighbors() { return json::array(); }
    json get_topology_payload() { return json::object(); }

    Config config_;
    std::map<std::string, OpHandler> handlers_;
    std::map<std::string, json> schemas_;
    std::map<std::string, SubscriptionCallback> subscriptions_;
    std::map<std::string, json> local_cache_;
    std::map<std::string, std::pair<std::vector<uint8_t>, std::string>> local_cache_binary_;
    std::map<std::string, bool> queryables_;
    void declare_queryable_for_path(const std::string& path);
    void* server_ptr_; 

    std::mutex handlers_mutex_;
    std::mutex storage_mutex_;

    std::map<std::string, uint64_t> fulfillment_counters_;
    std::map<std::string, double> total_latency_ms_;
    std::mutex counters_mutex_;
    void increment_fulfillment_counter(const std::string& path);
    void record_execution_metric(const std::string& path, double duration_ms);
    json get_fulfillment_counters();
    json get_fulfillment_latencies();

    struct CPUStats {
        uint64_t user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;
    };
    double get_free_cpu_percent();
    uint64_t get_free_memory_bytes();

    std::atomic<int> active_ops_count_{0};
    int max_concurrent_ops_ = 4;
    int get_active_ops_count() const { return active_ops_count_.load(); }
    int get_max_concurrent_ops() const { return max_concurrent_ops_; }
    void increment_active_ops() { active_ops_count_++; }
    void decrement_active_ops() { active_ops_count_--; }
    std::string get_machine_prefix() const;

    struct PeerInfo {
        std::string id;
        long long last_seen_ms = 0;
    };
    std::map<std::string, PeerInfo> peers_;
    std::mutex peers_mutex_;

private:
    CPUStats last_cpu_stats_;
    std::mutex cpu_mutex_;

    VFSResult read_cid_impl(const VFSRequest& req);
    VFSResult read_selector_impl(const VFSRequest& req);
};

// VfsRecord inline utility functions
inline std::vector<uint8_t> encode_record(const json& header, const std::vector<uint8_t>& payload = {}) {
    std::string header_str = header.dump();
    uint32_t header_len = header_str.size();
    
    std::vector<uint8_t> record(4 + header_len + payload.size());
    record[0] = (header_len >> 24) & 0xFF;
    record[1] = (header_len >> 16) & 0xFF;
    record[2] = (header_len >> 8) & 0xFF;
    record[3] = header_len & 0xFF;
    
    std::memcpy(record.data() + 4, header_str.data(), header_len);
    if (!payload.empty()) {
        std::memcpy(record.data() + 4 + header_len, payload.data(), payload.size());
    }
    return record;
}

inline bool decode_record(const std::vector<uint8_t>& record, json& header_out, std::vector<uint8_t>& payload_out) {
    if (record.size() < 4) return false;
    uint32_t header_len = ((uint32_t)record[0] << 24) |
                          ((uint32_t)record[1] << 16) |
                          ((uint32_t)record[2] << 8)  |
                          (uint32_t)record[3];
    if (record.size() < 4 + header_len) return false;
    
    std::string header_str((const char*)record.data() + 4, header_len);
    try {
        header_out = json::parse(header_str);
    } catch (...) {
        return false;
    }
    
    payload_out = std::vector<uint8_t>(record.begin() + 4 + header_len, record.end());
    return true;
}

// --- EXPLICIT SPECIALIZATION DECLARATIONS ---
template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const Selector& sel);
template<> json VFSNode::read<json>(const Selector& sel);
template<> double VFSNode::read<double>(const Selector& sel);
template<> int VFSNode::read<int>(const Selector& sel);
template<> std::string VFSNode::read<std::string>(const Selector& sel);
template<> VFSResult VFSNode::read<VFSResult>(const Selector& sel);

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const CID& cid);
template<> json VFSNode::read<json>(const CID& cid);
template<> double VFSNode::read<double>(const CID& cid);
template<> int VFSNode::read<int>(const CID& cid);
template<> std::string VFSNode::read<std::string>(const CID& cid);
template<> VFSResult VFSNode::read<VFSResult>(const CID& cid);

template<> std::vector<uint8_t> VFSNode::read<std::vector<uint8_t>>(const VFSRequest& req);
template<> json VFSNode::read<json>(const VFSRequest& req);
template<> double VFSNode::read<double>(const VFSRequest& req);
template<> int VFSNode::read<int>(const VFSRequest& req);
template<> std::string VFSNode::read<std::string>(const VFSRequest& req);
template<> VFSResult VFSNode::read<VFSResult>(const VFSRequest& req);

template<> CID VFSNode::materialize<std::vector<uint8_t>>(const std::vector<uint8_t>& data);
template<> CID VFSNode::materialize<json>(const json& data);
template<> CID VFSNode::materialize<std::string>(const std::string& data);
template<> CID VFSNode::materialize<jotcad::geo::Shape>(const jotcad::geo::Shape& data);

} // namespace fs
