#include "../vfs_node.h"
#include "../cid.h"
#include <zenoh.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <chrono>
#include <thread>
#include <future>
#include <sstream>
#include <random>

#include <list>

namespace fs {

#ifndef ZENOH_STATE_DEFINED
#define ZENOH_STATE_DEFINED
struct ZenohState {
    z_owned_session_t session;
    std::vector<z_owned_queryable_t> queryable_ops;
    z_owned_queryable_t queryable_cid;
    z_owned_queryable_t queryable_catalog;
    std::map<std::string, z_owned_subscriber_t> subscribers;
    std::list<std::string> queryable_keys;
    bool running = false;
    std::mutex mutex;
    std::condition_variable cv;
    std::unique_ptr<std::thread> advertising_thread;
};
#endif

VFSNode::VFSNode(const Config& config) : config_(config), server_ptr_(nullptr) {
    if (config_.storage_dir.empty()) {
        config_.storage_dir = ".vfs_storage_" + config_.id;
    }
    std::filesystem::create_directories(config_.storage_dir);

    json metrics_schema = {
        {"arguments", json::array()}
    };
    register_op("jot/vfs/metrics", [this](const VFSRequest& req) {
        json res = json::object();
        res["metrics"] = {
            {"fulfillment_counters", get_fulfillment_counters()},
            {"average_latencies_ms", get_fulfillment_latencies()},
            {"system", {
                {"free_cpu_percent", get_free_cpu_percent()},
                {"free_memory_bytes", get_free_memory_bytes()}
            }}
        };
        res["provider"] = config_.id;
        this->write(req.selector, res.dump());
    }, metrics_schema);
}

VFSNode::~VFSNode() {
    stop();
}

void VFSNode::register_op(const std::string& path, OpHandler handler, const json& schema) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    if (handlers_.find(path) != handlers_.end()) {
        throw std::runtime_error("VFS Operator Registration Collision: Operator path '" + path + "' is already registered!");
    }
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFSNode::increment_fulfillment_counter(const std::string& path) {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    fulfillment_counters_[path]++;
}

json VFSNode::get_fulfillment_counters() {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    json result = json::object();
    for (const auto& [path, count] : fulfillment_counters_) {
        result[path] = count;
    }
    return result;
}

void VFSNode::record_execution_metric(const std::string& path, double duration_ms) {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    fulfillment_counters_[path]++;
    total_latency_ms_[path] += duration_ms;
}

json VFSNode::get_fulfillment_latencies() {
    std::lock_guard<std::mutex> lock(counters_mutex_);
    json result = json::object();
    for (const auto& [path, total_latency] : total_latency_ms_) {
        uint64_t count = fulfillment_counters_[path];
        if (count > 0) {
            result[path] = total_latency / count;
        } else {
            result[path] = 0.0;
        } 
    }
    return result;
}

VFSResult VFSNode::read_cid_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) throw VFSException("Request expired", 408);
    }



    if (has_local(req.cid)) {
        auto res = get_local(req.cid);
        if (!res.data.empty() || res.metadata.value("state", "") == "AVAILABLE") return res;
    }
    if (req.localOnly) {
        throw VFSException("Content not found locally", 404);
    }

    // --- Zenoh Mesh Fetch ---
    if (!server_ptr_) {
        throw VFSException("VFS Node Zenoh session is not active", 500);
    }
    ZenohState* state = (ZenohState*)server_ptr_;
    
    std::string key = "jot/vfs/cid/" + req.cid;
    z_view_keyexpr_t q_ke;
    if (z_view_keyexpr_from_str(&q_ke, key.c_str()) < 0) {
        throw VFSException("Invalid Zenoh key: " + key, 400);
    }

    z_owned_fifo_handler_reply_t z_handler;
    z_owned_closure_reply_t closure;
    z_fifo_channel_reply_new(&closure, &z_handler, 16);

    z_get_options_t get_opts;
    z_get_options_default(&get_opts);
    get_opts.timeout_ms = 2000; // 2s default
    get_opts.target = Z_QUERY_TARGET_ALL;
    get_opts.consolidation.mode = Z_CONSOLIDATION_MODE_NONE;

    z_get(z_loan(state->session), z_loan(q_ke), nullptr, z_move(closure), &get_opts);

    z_owned_reply_t reply;
    VFSResult result;
    bool success = false;
    std::string err_msg = "Content not found for CID: " + req.cid;
    int err_code = 404;

    while (z_recv(z_loan(z_handler), &reply) == Z_OK) {
        if (z_reply_is_ok(z_loan(reply))) {
            const z_loaned_sample_t* sample = z_reply_ok(z_loan(reply));
            z_loaned_bytes_t const* payload_bytes = z_sample_payload(sample);
            size_t len = z_bytes_len(payload_bytes);
            
            std::vector<uint8_t> record_bytes(len);
            z_owned_slice_t slice;
            z_bytes_to_slice(payload_bytes, &slice);
            std::memcpy(record_bytes.data(), z_slice_data(z_loan(slice)), len);
            z_drop(z_move(slice));

            json rec_header;
            std::vector<uint8_t> rec_payload;
            if (decode_record(record_bytes, rec_header, rec_payload)) {
                int status = rec_header.value("status", 200);
                if (status == 200) {
                    result.data = rec_payload;
                    result.metadata = rec_header.value("metadata", json::object());
                    success = true;
                    // Write cache locally
                    write_local(req.cid, result.data, "", json::object());
                    break;
                } else if (status != 404) {
                    err_code = status;
                    err_msg = rec_header.value("error", "Remote fetch error");
                }
            }
        } else {
            const z_loaned_reply_err_t* err = z_reply_err(z_loan(reply));
            if (err != nullptr) {
                const z_loaned_bytes_t* payload = z_reply_err_payload(err);
                size_t len = z_bytes_len(payload);
                z_owned_slice_t slice;
                z_bytes_to_slice(payload, &slice);
                err_msg = std::string((const char*)z_slice_data(z_loan(slice)), len);
                z_drop(z_move(slice));
            }
        }
        z_drop(z_move(reply));
    }
    z_drop(z_move(z_handler));

    if (success) return result;
    throw VFSException(err_msg, err_code);
}

VFSResult VFSNode::read_selector_impl(const VFSRequest& req) {
    if (req.expiresAt > 0) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > req.expiresAt) throw VFSException("Request expired", 408);
    }

    std::string target_cid = get_cid(req.selector);



    if (has_local(target_cid)) {
        auto res = get_local(target_cid);
        if (req.followLinks && res.metadata.value("encoding", "") == "link") {
            if (std::find(req.resolutionStack.begin(), req.resolutionStack.end(), target_cid) != req.resolutionStack.end()) {
                throw VFSException("Infinite Link Cycle detected for CID: " + target_cid, 500);
            }
            try {
                json link_json = json::parse(res.data);
                VFSRequest linkReq = req;
                linkReq.op = "READ_SELECTOR";
                linkReq.cid = "";
                linkReq.selector = Selector::from_json(link_json);
                linkReq.resolutionStack.push_back(target_cid);
                return read_selector_impl(linkReq);
            } catch (...) {}
        }
        if (!res.data.empty() || res.metadata.value("state", "") == "AVAILABLE") {
            return res;
        }
    }

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        if (local_cache_.count(req.selector.path)) {
            VFSResult res;
            std::string s_val = local_cache_[req.selector.path].dump();
            res.data = std::vector<uint8_t>(s_val.begin(), s_val.end());
            res.metadata = {{"state", "AVAILABLE"}, {"encoding", "json"}};
            return res;
        }
        if (local_cache_binary_.count(req.selector.path)) {
            VFSResult res;
            res.data = local_cache_binary_[req.selector.path].first;
            res.metadata = {{"state", "AVAILABLE"}, {"encoding", "bytes"}};
            return res;
        }
    }

    // Check if operator is registered locally (supporting exact and wildcard matches)
    OpHandler handler;
    {   
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.count(req.selector.path)) {
            handler = handlers_[req.selector.path];
        } else {
            for (const auto& [reg_path, reg_handler] : handlers_) {
                if (reg_path.size() > 2 && reg_path.substr(reg_path.size() - 2) == "**") {
                    std::string prefix = reg_path.substr(0, reg_path.size() - 2);
                    if (req.selector.path.rfind(prefix, 0) == 0) {
                        handler = reg_handler;
                        break;
                    }
                }
            }
        }
    }
    if (handler) {
        auto start = std::chrono::high_resolution_clock::now();
        handler(req);
        auto end = std::chrono::high_resolution_clock::now();
        double duration_ms = std::chrono::duration<double, std::milli>(end - start).count();

        record_execution_metric(req.selector.path, duration_ms);
        
        // Publish update to VFS under the path: jot/vfs/metrics/<op_path>
        // Avoid recursion or publishing updates for the metrics calls themselves!
        if (req.selector.path.rfind("jot/vfs/metrics", 0) != 0) {
            std::string metric_path = "jot/vfs/metrics/" + req.selector.path;
            uint64_t count = 0;
            double avg_latency = 0.0;
            {
                std::lock_guard<std::mutex> lock(counters_mutex_);
                count = fulfillment_counters_[req.selector.path];
                double total = total_latency_ms_[req.selector.path];
                if (count > 0) {
                    avg_latency = total / count;
                }
            }
            json metric_payload = {
                {"path", req.selector.path},
                {"count", count},
                {"avg_latency_ms", avg_latency},
                {"provider", config_.id}
            };
            this->publish(metric_path, metric_payload);
        }

        if (has_local(target_cid)) return get_local(target_cid);
        throw VFSException("Handler failed to fulfill identity: " + target_cid);
    }
    if (req.localOnly) {
        throw VFSException("Content not found locally", 404);
    }

    // --- Zenoh Mesh Fetch ---
    if (!server_ptr_) {
        throw VFSException("VFS Node Zenoh session is not active", 500);
    }
    ZenohState* state = (ZenohState*)server_ptr_;

    // Gather targets to query (alphabetically sorted active machine IDs)
    std::vector<std::string> targets;
    {
        std::lock_guard<std::mutex> lock(peers_mutex_);
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        for (const auto& [peer_id, pi] : peers_) {
            if (now - pi.last_seen_ms < 10000) { // active within 10s
                targets.push_back(peer_id);
            }
        }
    }
    // Always include our own machine prefix as a fallback/target if we have the handler registered
    bool has_local_handler = false;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.find(req.selector.path) != handlers_.end()) {
            has_local_handler = true;
        }
    }
    if (has_local_handler) {
        targets.push_back(get_machine_prefix());
    }

    std::sort(targets.begin(), targets.end());

    // If no targets discovered, default to wildcard/broadcast
    if (targets.empty()) {
        targets.push_back("*");
    }

    // Serialize parameters (common to all queries)
    std::string query_params = "";
    bool first = true;
    for (auto it = req.selector.parameters.begin(); it != req.selector.parameters.end(); ++it) {
        if (!first) query_params += ";";
        first = false;
        query_params += it.key() + "=";
        std::string val;
        if (it.value().is_string()) val = it.value().get<std::string>();
        else val = it.value().dump();
        
        // URL encoder
        std::string encoded = "";
        for (char c : val) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                encoded += c;
            } else {
                char hex[4];
                std::snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
                encoded += hex;
            }
        }
        query_params += encoded;
    }
    if (!req.selector.output.empty()) {
        if (!first) query_params += ";";
        query_params += "output=" + req.selector.output;
    }

    VFSResult result;
    bool success = false;
    std::string err_msg = "Content not found for Selector: " + req.selector.path;
    int err_code = 404;

    for (const std::string& target_id : targets) {
        // A. If target is local, execute locally (concurrency limit permitting)
        if (target_id == get_machine_prefix()) {
            if (get_active_ops_count() < get_max_concurrent_ops()) {
                increment_active_ops();
                try {
                    VFSRequest localReq = req;
                    localReq.localOnly = true;
                    result = read_selector_impl(localReq);
                    success = true;
                } catch (const std::exception& e) {
                    err_code = 500;
                    err_msg = e.what();
                }
                decrement_active_ops();
                if (success) break;
            } else {
                std::cout << "[VFS Router] Local machine is busy, spilling over..." << std::endl;
                continue; // spill over to next target
            }
        }

        // B. Query targeted machine over Zenoh: "<target_id>/jot/vfs/op/<path>"
        std::string key = target_id + "/jot/vfs/op/" + req.selector.path;
        std::cout << "[VFS Router] Hammering target: '" << key << "'" << std::endl;

        z_view_keyexpr_t q_ke;
        if (z_view_keyexpr_from_str(&q_ke, key.c_str()) < 0) {
            continue;
        }

        z_owned_fifo_handler_reply_t z_handler;
        z_owned_closure_reply_t closure;
        z_fifo_channel_reply_new(&closure, &z_handler, 16);

        z_get_options_t get_opts;
        z_get_options_default(&get_opts);
        get_opts.timeout_ms = 3000; // 3-second timeout per target query

        z_get(z_loan(state->session), z_loan(q_ke), query_params.empty() ? nullptr : query_params.c_str(), z_move(closure), &get_opts);

        z_owned_reply_t reply;

        while (z_recv(z_loan(z_handler), &reply) == Z_OK) {
            if (!success && z_reply_is_ok(z_loan(reply))) {
                const z_loaned_sample_t* sample = z_reply_ok(z_loan(reply));
                z_loaned_bytes_t const* payload_bytes = z_sample_payload(sample);
                size_t len = z_bytes_len(payload_bytes);
                
                std::vector<uint8_t> record_bytes(len);
                z_owned_slice_t slice;
                z_bytes_to_slice(payload_bytes, &slice);
                std::memcpy(record_bytes.data(), z_slice_data(z_loan(slice)), len);
                z_drop(z_move(slice));

                json rec_header;
                std::vector<uint8_t> rec_payload;
                if (decode_record(record_bytes, rec_header, rec_payload)) {
                    int status = rec_header.value("status", 200);
                    if (status == 200) {
                        result.data = rec_payload;
                        result.metadata = rec_header.value("metadata", json::object());
                        success = true;
                        write_local(target_cid, result.data, req.selector.path, req.selector.parameters);
                    } else if (status == 503 || status == 429) {
                        std::cout << "[VFS Router] Target '" << target_id << "' rejected (Busy). Trying next target..." << std::endl;
                    } else {
                        err_code = status;
                        err_msg = rec_header.value("error", "Remote computation error");
                    }
                }
            }
            z_drop(z_move(reply));
        }

        z_drop(z_move(z_handler));

        if (success) {
            break; // Query succeeded, exit targets loop
        }
    }
    if (success) return result;
    throw VFSException(err_msg, err_code);
}

std::string VFSNode::get_cid(const Selector& sel) {
    // Protocol Rule: Hash the canonical binary representation (JCB) directly.
    return vfs_hash256(encode_jcb(sel.to_json()));
}

bool VFSNode::has_local(const std::string& cid) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
    return std::filesystem::exists(p) || std::filesystem::exists(mp);
}

VFSResult VFSNode::get_local(const std::string& cid) {
    VFSResult res;
    res.metadata = {{"state", "PENDING"}, {"cid", cid}};

    std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    if (std::filesystem::exists(mp)) {
        std::ifstream in(mp);
        try { in >> res.metadata; } catch(...) {}
    }

    if (std::filesystem::exists(dp)) {
        std::ifstream in(dp, std::ios::binary);
        res.data = std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        res.metadata["state"] = "AVAILABLE";
    }

    return res;
}

Selector VFSNode::write_bytes(const Selector& sel, const std::vector<uint8_t>& data) {
    std::string cid = get_cid(sel);
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());

        json meta = {
            {"state", "AVAILABLE"},
            {"encoding", "bytes"},
            {"selector", sel.to_json()}
        };
        std::ofstream mos(mp);
        mos << meta.dump();
    }
    return sel;
}

void VFSNode::link(const Selector& src, const Selector& tgt) {
    std::string srcKey = get_cid(src);
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    std::filesystem::path dp = std::filesystem::path(config_.storage_dir) / (srcKey + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (srcKey + ".meta");
    
    {
        std::ofstream os(dp);
        os << tgt.to_json().dump();
    }

    json meta = {
        {"selector", src.to_json()},
        {"state", "AVAILABLE"},
        {"encoding", "link"}
    };
    std::ofstream os(mp);
    os << meta.dump();
}

void VFSNode::notify(const Selector& selector, const json& payload, const std::vector<std::string>& stack) {
    if (!server_ptr_) return;
    ZenohState* state = (ZenohState*)server_ptr_;
    
    std::string key = "jot/vfs/pub/" + selector.path;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, key.c_str()) < 0) return;
    
    std::string payload_str;
    if (payload.is_binary()) {
        auto bin = payload.get_binary();
        payload_str = std::string(bin.begin(), bin.end());
    } else {
        payload_str = payload.dump();
    }
    
    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, (const uint8_t*)payload_str.data(), payload_str.size());
    
    z_put_options_t opts;
    z_put_options_default(&opts);
    
    z_put(z_loan(state->session), z_loan(ke), z_move(bytes), &opts);
}

void VFSNode::write_local(const std::string& cid, const std::vector<uint8_t>& data, const std::string& path, const json& params) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (cid + ".meta");
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    if (!data.empty()) {
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)data.data(), data.size());
    }
    
    json meta = {
        {"state", "AVAILABLE"},
        {"encoding", "json"},
        {"selector", {{"path", path}, {"parameters", params}}}
    };
    std::ofstream mos(mp);
    mos << meta.dump();
}

void VFSNode::write_local_link(const std::string& src_cid, const std::string& src_path, const json& src_params, const std::string& tgt_path, const json& tgt_params) {
    std::filesystem::path p = std::filesystem::path(config_.storage_dir) / (src_cid + ".data");
    std::filesystem::path mp = std::filesystem::path(config_.storage_dir) / (src_cid + ".meta");
    
    std::lock_guard<std::mutex> lock(storage_mutex_);
    
    json tgt_sel = {{"path", tgt_path}, {"parameters", tgt_params}};
    std::string tgt_str = tgt_sel.dump();
    std::ofstream os(p, std::ios::binary);
    os.write(tgt_str.data(), tgt_str.size());
    
    json meta = {
        {"state", "AVAILABLE"},
        {"encoding", "link"},
        {"selector", {{"path", src_path}, {"parameters", src_params}}}
    };
    std::ofstream mos(mp);
    mos << meta.dump();
}

json VFSNode::get_catalog() {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    json catalog = json::object();
    for (auto const& [path, schema] : schemas_) {
        catalog[path] = schema;
    }
    return {{"catalog", catalog}, {"provider", config_.id}};
}

void VFSNode::publish(const std::string& path, const json& payload) {
    if (!server_ptr_) return;
    ZenohState* state = (ZenohState*)server_ptr_;
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        local_cache_[path] = payload;
    }

    bool declare_now = false;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (queryables_.find(path) == queryables_.end()) {
            queryables_[path] = true;
            declare_now = true;
        }
    }
    if (declare_now) {
        declare_queryable_for_path(path);
    }

    std::string key = get_machine_prefix() + "/jot/vfs/pub/" + path;
    std::cout << "[VFSNode " << config_.id << "] publish key: " << key << " payload: " << payload.dump() << std::endl;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, key.c_str()) < 0) return;
    
    std::string payload_str;
    if (payload.is_binary()) {
        auto bin = payload.get_binary();
        payload_str = std::string(bin.begin(), bin.end());
    } else {
        payload_str = payload.dump();
    }
    
    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, (const uint8_t*)payload_str.data(), payload_str.size());
    
    z_put_options_t opts;
    z_put_options_default(&opts);
    
    z_put(z_loan(state->session), z_loan(ke), z_move(bytes), &opts);
}

void VFSNode::publish_binary(const std::string& path, const uint8_t* data, size_t len) {
    if (!server_ptr_) return;
    ZenohState* state = (ZenohState*)server_ptr_;
    
    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        local_cache_binary_[path] = std::make_pair(std::vector<uint8_t>(data, data + len), "application/octet-stream");
    }

    bool declare_now = false;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (queryables_.find(path) == queryables_.end()) {
            queryables_[path] = true;
            declare_now = true;
        }
    }
    if (declare_now) {
        declare_queryable_for_path(path);
    }

    std::string key = get_machine_prefix() + "/jot/vfs/pub/" + path;
    std::cout << "[VFSNode " << config_.id << "] publish_binary key: " << key << std::endl;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, key.c_str()) < 0) return;
    
    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, data, len);
    
    z_put_options_t opts;
    z_put_options_default(&opts);
    
    z_put(z_loan(state->session), z_loan(ke), z_move(bytes), &opts);
}

void VFSNode::subscribe(const std::string& path, SubscriptionCallback callback) {
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        subscriptions_[path] = callback;
    }
    
    if (server_ptr_) {
        ZenohState* state = (ZenohState*)server_ptr_;
        std::string key = "*/jot/vfs/pub/" + path;
        std::cout << "[VFSNode " << config_.id << "] subscribe key: " << key << std::endl;
        const char* key_cstr = nullptr;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->queryable_keys.push_back(key);
            key_cstr = state->queryable_keys.back().c_str();
        }
        
        z_view_keyexpr_t ke_sub;
        z_view_keyexpr_from_str(&ke_sub, key_cstr);
        
        auto* cb_ctx = new SubscriptionCallback(callback);
        
        z_owned_closure_sample_t cb_sample;
        z_closure_sample(&cb_sample, [](struct z_loaned_sample_t* sample, void* context) {
            auto* cb = static_cast<SubscriptionCallback*>(context);
            z_owned_string_t payload_str;
            z_bytes_to_string(z_sample_payload(sample), &payload_str);
            std::string raw_msg(z_string_data(z_string_loan(&payload_str)), z_string_len(z_string_loan(&payload_str)));
            z_string_drop(z_string_move(&payload_str));
            try {
                json parsed = json::parse(raw_msg);
                (*cb)(parsed);
            } catch (...) {
                (*cb)(json(raw_msg));
            }
        }, [](void* context) {
            delete static_cast<SubscriptionCallback*>(context);
        }, cb_ctx);
        
        z_owned_subscriber_t subscriber;
        z_subscriber_options_t opts;
        z_subscriber_options_default(&opts);
        
        if (z_declare_subscriber(z_loan(state->session), &subscriber, z_loan(ke_sub), z_move(cb_sample), &opts) == Z_OK) {
            z_owned_subscriber_t old_subscriber;
            bool has_old = false;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                auto it = state->subscribers.find(path);
                if (it != state->subscribers.end()) {
                    old_subscriber = it->second;
                    has_old = true;
                }
                state->subscribers[path] = subscriber;
            }
            if (has_old) {
                z_undeclare_subscriber(z_move(old_subscriber));
            }
        } else {
            std::cerr << "[VFSNode " << config_.id << "] Failed declaring subscriber dynamically for: " << key << std::endl;
            delete cb_ctx;
        }
    }
}

void VFSNode::unlisten(const std::string& path) {
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        subscriptions_.erase(path);
    }
    
    if (server_ptr_) {
        ZenohState* state = (ZenohState*)server_ptr_;
        z_owned_subscriber_t subscriber_to_undeclare;
        bool has_subscriber = false;
        {
            std::lock_guard<std::mutex> lock_state(state->mutex);
            auto it = state->subscribers.find(path);
            if (it != state->subscribers.end()) {
                subscriber_to_undeclare = it->second;
                state->subscribers.erase(it);
                has_subscriber = true;
            }
        }
        if (has_subscriber) {
            z_undeclare_subscriber(z_move(subscriber_to_undeclare));
        }
    }
}

double VFSNode::get_free_cpu_percent() {
#ifdef __linux__
    std::lock_guard<std::mutex> lock(cpu_mutex_);
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 100.0;
    std::string line;
    if (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string cpu;
        if (ss >> cpu && cpu == "cpu") {
            uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
            if (ss >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal) {
                uint64_t current_idle = idle + iowait;
                uint64_t current_non_idle = user + nice + system + irq + softirq + steal;
                uint64_t current_total = current_idle + current_non_idle;
                
                uint64_t prev_idle = last_cpu_stats_.idle + last_cpu_stats_.iowait;
                uint64_t prev_non_idle = last_cpu_stats_.user + last_cpu_stats_.nice + 
                                         last_cpu_stats_.system + last_cpu_stats_.irq + 
                                         last_cpu_stats_.softirq + last_cpu_stats_.steal;
                uint64_t prev_total = prev_idle + prev_non_idle;
                
                last_cpu_stats_ = {user, nice, system, idle, iowait, irq, softirq, steal};
                
                uint64_t total_diff = current_total - prev_total;
                uint64_t idle_diff = current_idle - prev_idle;
                
                if (total_diff > 0) {
                    double cpu_usage = 100.0 * (total_diff - idle_diff) / total_diff;
                    if (cpu_usage < 0.0) cpu_usage = 0.0;
                    if (cpu_usage > 100.0) cpu_usage = 100.0;
                    return 100.0 - cpu_usage;
                }
            }
        }
    }
    return 100.0;
#else
    return 100.0;
#endif
}

uint64_t VFSNode::get_free_memory_bytes() {
#ifdef __linux__
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return 0;
    std::string line;
    uint64_t mem_free_kb = 0;
    uint64_t mem_available_kb = 0;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string key;
        uint64_t value;
        std::string unit;
        if (ss >> key >> value >> unit) {
            if (key == "MemAvailable:") {
                mem_available_kb = value;
            } else if (key == "MemFree:") {
                mem_free_kb = value;
            }
        }
    }
    if (mem_available_kb > 0) return mem_available_kb * 1024;
    if (mem_free_kb > 0) return mem_free_kb * 1024;
    return 0;
#else
    return 0;
#endif
}

json VFSNode::read(const std::string& path) {
    return read<json>(Selector(path));
}

std::string VFSNode::get_machine_prefix() const {
    if (const char* env_machine = std::getenv("MACHINE_ID")) {
        std::string m(env_machine);
        if (!m.empty()) return m;
    }
    if (const char* env_host = std::getenv("HOSTNAME")) {
        std::string h(env_host);
        if (!h.empty()) return h;
    }
    return config_.id;
}

} // namespace fs
