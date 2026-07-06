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
};
#endif

VFSNode::VFSNode(const Config& config) : config_(config), server_ptr_(nullptr) {
    if (config_.storage_dir.empty()) {
        config_.storage_dir = ".vfs_storage_" + config_.id;
    }
    std::filesystem::create_directories(config_.storage_dir);
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

    // Check if operator is registered locally
    OpHandler handler;
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        if (handlers_.count(req.selector.path)) {
            handler = handlers_[req.selector.path];
        }
    }
    if (handler) {
        handler(req);
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
    
    // Construct Zenoh key: "jot/vfs/op/<path>"
    std::string key = "jot/vfs/op/" + req.selector.path;
    
    // Serialize params: subject=CID_BOX&tool=CID_SPHERE
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

    z_view_keyexpr_t q_ke;
    if (z_view_keyexpr_from_str(&q_ke, key.c_str()) < 0) {
        throw VFSException("Invalid Zenoh key: " + key, 400);
    }

    z_owned_fifo_handler_reply_t z_handler;
    z_owned_closure_reply_t closure;
    z_fifo_channel_reply_new(&closure, &z_handler, 16);

    z_get_options_t get_opts;
    z_get_options_default(&get_opts);
    get_opts.timeout_ms = 10000;

    z_get(z_loan(state->session), z_loan(q_ke), query_params.empty() ? nullptr : query_params.c_str(), z_move(closure), &get_opts);

    z_owned_reply_t reply;
    VFSResult result;
    bool success = false;
    std::string err_msg = "Content not found for Selector: " + req.selector.path;
    int err_code = 404;

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
                    // Write cache locally
                    write_local(target_cid, result.data, req.selector.path, req.selector.parameters);
                } else {
                    err_code = status;
                    err_msg = rec_header.value("error", "Remote computation error");
                }
            }
        }
        z_drop(z_move(reply));
    }
    z_drop(z_move(z_handler));

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

    std::string key = "jot/vfs/pub/" + path;
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

    std::string key = "jot/vfs/pub/" + path;
    z_view_keyexpr_t ke;
    if (z_view_keyexpr_from_str(&ke, key.c_str()) < 0) return;
    
    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, data, len);
    
    z_put_options_t opts;
    z_put_options_default(&opts);
    
    z_put(z_loan(state->session), z_loan(ke), z_move(bytes), &opts);
}

void VFSNode::subscribe(const std::string& path, SubscriptionCallback callback) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    subscriptions_[path] = callback;
    
    if (server_ptr_) {
        ZenohState* state = (ZenohState*)server_ptr_;
        std::string key = "jot/vfs/pub/" + path;
        state->queryable_keys.push_back(key);
        const std::string& persistent_key = state->queryable_keys.back();
        
        z_view_keyexpr_t ke_sub;
        z_view_keyexpr_from_str(&ke_sub, persistent_key.c_str());
        
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
            auto it = state->subscribers.find(path);
            if (it != state->subscribers.end()) {
                z_undeclare_subscriber(z_move(it->second));
            }
            state->subscribers[path] = subscriber;
        } else {
            std::cerr << "[VFSNode " << config_.id << "] Failed declaring subscriber dynamically for: " << persistent_key << std::endl;
            delete cb_ctx;
        }
    }
}

void VFSNode::unlisten(const std::string& path) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);
    subscriptions_.erase(path);
    
    if (server_ptr_) {
        ZenohState* state = (ZenohState*)server_ptr_;
        auto it = state->subscribers.find(path);
        if (it != state->subscribers.end()) {
            z_undeclare_subscriber(z_move(it->second));
            state->subscribers.erase(it);
        }
    }
}

json VFSNode::read(const std::string& path) {
    return read<json>(Selector(path));
}

} // namespace fs
