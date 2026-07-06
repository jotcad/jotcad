#include "../vfs_node.h"
#include "../cid.h"
#include <zenoh.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <iomanip>
#include <list>
#include <map>
#include <fstream>

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

// URL Decoder helper
static std::string url_decode(const std::string& val) {
    std::string decoded = "";
    for (size_t i = 0; i < val.length(); ++i) {
        if (val[i] == '%' && i + 2 < val.length()) {
            char hex[3] = { val[i+1], val[i+2], 0 };
            decoded += (char)std::strtol(hex, nullptr, 16);
            i += 2;
        } else if (val[i] == '+') {
            decoded += ' ';
        } else {
            decoded += val[i];
        }
    }
    return decoded;
}

// URL Parameter Parser helper with schema awareness
static json parse_query_params(const std::string& query, const std::map<std::string, std::string>& arg_types, std::string& output_out, long long& expiresAt_out) {
    json params = json::object();
    std::stringstream ss(query);
    std::string item;
    while (std::getline(ss, item, ';')) {
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string key = item.substr(0, eq);
            std::string val = url_decode(item.substr(eq + 1));
            if (key == "output") {
                output_out = val;
            } else if (key == "expiresAt") {
                try {
                    expiresAt_out = std::stoll(val);
                } catch (...) {
                    expiresAt_out = 0;
                }
            } else {
                bool is_string = false;
                if (arg_types.count(key)) {
                    std::string type = arg_types.at(key);
                    if (type == "jot:string" || type == "string") {
                        is_string = true;
                    }
                }

                if (is_string) {
                    params[key] = val;
                } else {
                    try {
                        params[key] = json::parse(val);
                    } catch (...) {
                        params[key] = val;
                    }
                }
            }
        }
    }
    return params;
}

// Zero-copy vector deleter for Zenoh bytes
static void delete_vector_u8(void* data, void* context) {
    delete static_cast<std::vector<uint8_t>*>(context);
}

// Operator Query Handler
static void query_handler_op(z_loaned_query_t* query, void* context) {
    VFSNode* node = static_cast<VFSNode*>(context);
    
    // Extract key expression
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));
    
    // Extract parameters
    z_view_string_t params_str;
    z_query_parameters(query, &params_str);
    std::string params(z_string_data(z_loan(params_str)), z_string_len(z_loan(params_str)));
    std::cout << "[VFS Server] Raw parameters string received: " << params << std::endl;

    // Strip "jot/vfs/op/" prefix (length 11) to match registered paths
    std::string op_path = (key.length() > 11) ? key.substr(11) : key;

    // Look up schema to extract argument types
    std::map<std::string, std::string> arg_types;
    {
        std::lock_guard<std::mutex> lock(node->handlers_mutex_);
        if (node->schemas_.count(op_path)) {
            const auto& schema = node->schemas_[op_path];
            if (schema.contains("arguments") && schema["arguments"].is_array()) {
                for (const auto& arg : schema["arguments"]) {
                    if (arg.contains("name") && arg["name"].is_string() && arg.contains("type") && arg["type"].is_string()) {
                        arg_types[arg["name"].get<std::string>()] = arg["type"].get<std::string>();
                    }
                }
            }
        }
    }

    z_owned_query_t query_owned;
    z_query_clone(&query_owned, query);

    std::thread([node, query_owned = std::move(query_owned), op_path, key, params, arg_types]() mutable {
        try {
            VFSNode::VFSRequest req;
            req.op = "READ_SELECTOR";
            std::string output = "";
            long long expiresAt = 0;
            json parsed_params = parse_query_params(params, arg_types, output, expiresAt);
            req.selector = Selector(op_path, parsed_params, output);
            req.selector.validate();
            req.expiresAt = expiresAt;
            req.localOnly = true; // Queryable handler only services local resources
            
            // Execute the handler
            VFSResult result = node->read<VFSResult>(req);
            
            json resp_header = {
                {"status", 200},
                {"metadata", result.metadata},
                {"encoding", result.metadata.value("encoding", "json")}
            };
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header, result.data));
            
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            
            z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
        } catch (const VFSException& e) {
            if (e.code == 404) {
                std::cout << "[VFS Server] query_handler_op not found locally for path: '" << op_path << "'. Silently ignoring to let other nodes reply." << std::endl;
                z_drop(z_move(query_owned));
                return;
            }
            std::string err_msg = "[" + node->config_.id + "] " + e.what();
            json resp_header = {
                {"status", e.code},
                {"error", err_msg},
                {"metadata", {{"state", "ERROR"}}},
                {"encoding", "json"}
            };
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header));
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
        } catch (const std::exception& e) {
            std::string err_msg = "[" + node->config_.id + "] " + e.what();
            json resp_header = {
                {"status", 500},
                {"error", err_msg},
                {"metadata", {{"state", "ERROR"}}},
                {"encoding", "json"}
            };
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header));
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
        }
        z_drop(z_move(query_owned));
    }).detach();
}

// Content (CID) Query Handler
static void query_handler_cid(z_loaned_query_t* query, void* context) {
    VFSNode* node = static_cast<VFSNode*>(context);
    
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));
    std::cout << "[VFS Server] query_handler_cid received query for key: '" << key << "' on node '" << node->config_.id << "'" << std::endl;

    // Strip "jot/vfs/cid/" prefix (length 12) to get raw CID
    std::string cid = (key.length() > 12) ? key.substr(12) : key;

    z_owned_query_t query_owned;
    z_query_clone(&query_owned, query);

    std::thread([node, query_owned = std::move(query_owned), cid, key]() mutable {
        try {
            VFSNode::VFSRequest req;
            req.op = "READ_CID";
            req.cid = cid;
            req.localOnly = true; // Queryable handler only services local resources

            VFSResult result = node->read<VFSResult>(req);
            
            json resp_header = {
                {"status", 200},
                {"metadata", result.metadata},
                {"encoding", result.metadata.value("encoding", "json")}
            };
            std::cout << "[VFS Server] query_handler_cid replying 200 for CID: '" << cid << "' on node '" << node->config_.id << "' with data size: " << result.data.size() << std::endl;
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header, result.data));
            
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            
            int ret = z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
            std::cout << "[VFS Server] query_handler_cid z_query_reply returned: " << ret << " for CID: " << cid << std::endl;
        } catch (const VFSException& e) {
            if (e.code == 404) {
                std::cout << "[VFS Server] query_handler_cid not found locally for CID: '" << cid << "'. Replying 404." << std::endl;
                json resp_header = {
                    {"status", 404},
                    {"error", "CID not found locally: " + cid},
                    {"metadata", {{"state", "ERROR"}}},
                    {"encoding", "json"}
                };
                auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header));
                z_owned_bytes_t reply_payload;
                z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
                
                z_query_reply_options_t options;
                z_query_reply_options_default(&options);
                z_view_keyexpr_t reply_keyexpr;
                z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
                z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
            } else {
                std::cout << "[VFS Server] VFSException in query_handler_cid: " << e.what() << std::endl;
                std::string err_msg = "[" + node->config_.id + "] " + e.what();
                json resp_header = {
                    {"status", e.code},
                    {"error", err_msg},
                    {"metadata", {{"state", "ERROR"}}},
                    {"encoding", "json"}
                };
                auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header));
                z_owned_bytes_t reply_payload;
                z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
                
                z_query_reply_options_t options;
                z_query_reply_options_default(&options);
                z_view_keyexpr_t reply_keyexpr;
                z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
                z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
            }
        } catch (const std::exception& e) {
            std::cout << "[VFS Server] Exception in query_handler_cid: " << e.what() << std::endl;
            std::string err_msg = "[" + node->config_.id + "] " + e.what();
            json resp_header = {
                {"status", 500},
                {"error", err_msg},
                {"metadata", {{"state", "ERROR"}}},
                {"encoding", "json"}
            };
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header));
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
        }
        z_drop(z_move(query_owned));
    }).detach();
}

static void query_handler_catalog(z_loaned_query_t* query, void* context) {
    auto* node = static_cast<VFSNode*>(context);
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));

    z_owned_query_t query_owned;
    z_query_clone(&query_owned, query);

    std::thread([node, query_owned = std::move(query_owned), key]() mutable {
        try {
            json catalog = node->get_catalog();
            json resp_header = {
                {"status", 200},
                {"metadata", {{"state", "AVAILABLE"}, {"encoding", "json"}}},
                {"encoding", "json"}
            };
            std::string payload_str = catalog.dump();
            std::vector<uint8_t> payload_bytes(payload_str.begin(), payload_str.end());
            auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header, payload_bytes));
            
            z_owned_bytes_t reply_payload;
            z_bytes_from_buf(&reply_payload, record_bytes->data(), record_bytes->size(), delete_vector_u8, record_bytes);
            
            z_query_reply_options_t options;
            z_query_reply_options_default(&options);
            
            z_view_keyexpr_t reply_keyexpr;
            z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());
            
            z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
        } catch (...) {}
        z_drop(z_move(query_owned));
    }).detach();
}

inline bool file_exists_helper(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

void VFSNode::listen() {
    std::cout << "[VFSNode " << config_.id << "] Initializing Zenoh Session..." << std::endl;
    
    std::string cert_path = "";
    std::string key_path = "";
    bool use_tls = false;

    const char* env_cert = std::getenv("SSL_CERT_PATH");
    const char* env_key = std::getenv("SSL_KEY_PATH");

    if (env_cert && env_key) {
        std::string s_cert(env_cert);
        std::string s_key(env_key);
        if (!s_cert.empty() && !s_key.empty()) {
            if (file_exists_helper(s_cert) && file_exists_helper(s_key)) {
                cert_path = s_cert;
                key_path = s_key;
                use_tls = true;
            }
        }
    } else {
        std::string c_path = ".ssl/localhost-cert.pem";
        std::string k_path = ".ssl/localhost-key.pem";
        if (!file_exists_helper(c_path)) {
            c_path = "../.ssl/localhost-cert.pem";
            k_path = "../.ssl/localhost-key.pem";
            if (!file_exists_helper(c_path)) {
                c_path = "../../.ssl/localhost-cert.pem";
                k_path = "../../.ssl/localhost-key.pem";
            }
        }
        if (file_exists_helper(c_path) && file_exists_helper(k_path)) {
            cert_path = c_path;
            key_path = k_path;
            use_tls = true;
        }
    }

    // Construct configuration JSON dynamically, disabling multicast scouting unless a custom address is provided
    std::string json_str = "{";
    if (const char* env_multicast = std::getenv("ZENOH_MULTICAST_ADDRESS")) {
        std::cout << "[VFSNode " << config_.id << "] Setting custom multicast scouting address: " << env_multicast << std::endl;
        json_str += "\"scouting\": {\"multicast\": {\"address\": \"" + std::string(env_multicast) + "\", \"enabled\": true}}";
    } else {
        std::cout << "[VFSNode " << config_.id << "] Disabling multicast scouting to ensure environment isolation." << std::endl;
        json_str += "\"scouting\": {\"multicast\": {\"enabled\": false}}";
    }

    if (use_tls) {
        std::cout << "[VFSNode " << config_.id << "] Found SSL certificates. Enabling TLS/WSS transport encryption." << std::endl;
        json_str += ", \"listen\": {\"endpoints\": [\"tls/0.0.0.0:" + std::to_string(config_.port) + "\", \"ws/0.0.0.0:" + std::to_string(config_.port + 1000) + "\"]}";
    } else {
        std::cout << "[VFSNode " << config_.id << "] No SSL certificates found. Using unencrypted tcp/ws transport." << std::endl;
        json_str += ", \"listen\": {\"endpoints\": [\"tcp/0.0.0.0:" + std::to_string(config_.port) + "\", \"ws/0.0.0.0:" + std::to_string(config_.port + 1000) + "\"]}";
    }

    if (!config_.neighbors.empty()) {
        json_str += ", \"connect\": {\"endpoints\": [";
        for (size_t i = 0; i < config_.neighbors.size(); ++i) {
            std::string n = config_.neighbors[i];
            if (n.rfind("http://", 0) == 0) {
                n = "tcp/" + n.substr(7);
            } else if (n.rfind("https://", 0) == 0) {
                n = "tls/" + n.substr(8); // Map https to tls
            } else if (n.rfind("tcp/", 0) != 0 && n.rfind("ws/", 0) != 0 && n.rfind("tls/", 0) != 0 && n.rfind("wss/", 0) != 0) {
                n = (use_tls ? "tls/" : "tcp/") + n;
            }
            json_str += (i == 0 ? "" : ", ") + ("\"" + n + "\"");
        }
        json_str += "]}";
    }

    if (use_tls) {
        json_str += ", \"transport\": { \"link\": { \"tls\": { \"listen_certificate\": \"" + cert_path + "\", \"listen_private_key\": \"" + key_path + "\" } } }";
    }
    json_str += "}";

    z_owned_config_t config;
    if (zc_config_from_str(&config, json_str.c_str()) != Z_OK) {
        std::cerr << "[VFSNode " << config_.id << "] Failed parsing custom configuration, falling back to defaults. JSON was: " << json_str << std::endl;
        z_config_default(&config);
    }

    ZenohState* state = new ZenohState();
    if (z_open(&state->session, z_move(config), NULL) != Z_OK) {
        std::cerr << "[VFSNode " << config_.id << "] CRITICAL: Failed to open Zenoh session!" << std::endl;
        delete state;
        return;
    }
    server_ptr_ = state;

    // 1. Declare operator fulfillment queryables for all registered handlers
    for (const auto& [op_path, handler] : handlers_) {
        std::string key = "jot/vfs/op/" + op_path;
        state->queryable_keys.push_back(key);
        const std::string& persistent_key = state->queryable_keys.back();

        z_view_keyexpr_t ke_op;
        z_view_keyexpr_from_str(&ke_op, persistent_key.c_str());
        z_owned_closure_query_t cb_op;
        z_closure(&cb_op, query_handler_op, nullptr, this);
        z_queryable_options_t opts_op;
        z_queryable_options_default(&opts_op);

        z_owned_queryable_t queryable;
        if (z_declare_queryable(z_loan(state->session), &queryable, z_loan(ke_op), z_move(cb_op), &opts_op) != Z_OK) {
            std::cerr << "[VFSNode " << config_.id << "] Failed declaring operator queryable for: " << persistent_key << std::endl;
        } else {
            state->queryable_ops.push_back(queryable);
        }
    }

    // 2. Declare content (CID) queryable on: jot/vfs/cid/**
    z_view_keyexpr_t ke_cid;
    z_view_keyexpr_from_str(&ke_cid, "jot/vfs/cid/**");
    z_owned_closure_query_t cb_cid;
    z_closure(&cb_cid, query_handler_cid, nullptr, this);
    z_queryable_options_t opts_cid;
    z_queryable_options_default(&opts_cid);

    if (z_declare_queryable(z_loan(state->session), &state->queryable_cid, z_loan(ke_cid), z_move(cb_cid), &opts_cid) != Z_OK) {
        std::cerr << "[VFSNode " << config_.id << "] Failed declaring content queryable!" << std::endl;
    }

    // 3. Declare catalog queryable on: jot/vfs/catalog
    z_view_keyexpr_t ke_cat;
    z_view_keyexpr_from_str(&ke_cat, "jot/vfs/catalog");
    z_owned_closure_query_t cb_cat;
    z_closure(&cb_cat, query_handler_catalog, nullptr, this);
    z_queryable_options_t opts_cat;
    z_queryable_options_default(&opts_cat);

    if (z_declare_queryable(z_loan(state->session), &state->queryable_catalog, z_loan(ke_cat), z_move(cb_cat), &opts_cat) != Z_OK) {
        std::cerr << "[VFSNode " << config_.id << "] Failed declaring catalog queryable!" << std::endl;
    }

    // 4. Declare subscribers for all registered callbacks
    for (const auto& [sub_path, callback] : subscriptions_) {
        std::string key = "jot/vfs/pub/" + sub_path;
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
            state->subscribers[sub_path] = subscriber;
        } else {
            std::cerr << "[VFSNode " << config_.id << "] Failed declaring subscriber on startup for: " << persistent_key << std::endl;
            delete cb_ctx;
        }
    }

    std::cout << "[VFSNode " << config_.id << "] Zenoh listener successfully started on port " << config_.port << std::endl;

    // Block the thread until stop() is called
    {
        std::unique_lock<std::mutex> lock(state->mutex);
        state->running = true;
        state->cv.wait(lock, [state] { return !state->running; });
    }
    
    std::cout << "[VFSNode " << config_.id << "] Zenoh listener thread exiting." << std::endl;
    delete state;
}

void VFSNode::stop() {
    if (server_ptr_) {
        ZenohState* state = static_cast<ZenohState*>(server_ptr_);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (!state->running) return;
            state->running = false;
        }
        
        std::cout << "[VFSNode " << config_.id << "] Shutting down Zenoh Session..." << std::endl;
        
        for (auto& pair : state->subscribers) {
            z_undeclare_subscriber(z_move(pair.second));
        }
        state->subscribers.clear();

        for (auto& q : state->queryable_ops) {
            z_undeclare_queryable(z_move(q));
        }
        state->queryable_ops.clear();
        z_undeclare_queryable(z_move(state->queryable_cid));
        z_undeclare_queryable(z_move(state->queryable_catalog));
        z_close(z_loan_mut(state->session), NULL);
        z_drop(z_move(state->session));
        
        state->cv.notify_all();
        server_ptr_ = nullptr;
    }
}

void VFSNode::declare_queryable_for_path(const std::string& path) {
    if (!server_ptr_) return;
    ZenohState* state = static_cast<ZenohState*>(server_ptr_);

    std::string key = "jot/vfs/op/" + path;
    state->queryable_keys.push_back(key);
    const std::string& persistent_key = state->queryable_keys.back();

    z_view_keyexpr_t ke_op;
    z_view_keyexpr_from_str(&ke_op, persistent_key.c_str());
    
    z_owned_closure_query_t cb_op;
    z_closure(&cb_op, [](z_loaned_query_t* query, void* context) {
        VFSNode* self = static_cast<VFSNode*>(context);
        
        z_view_string_t key_string;
        z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
        std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));
        
        // Strip "jot/vfs/op/" (length 11) to get path
        std::string path = (key.length() > 11) ? key.substr(11) : key;

        z_owned_query_t query_owned;
        z_query_clone(&query_owned, query);

        std::thread([self, query_owned = std::move(query_owned), path, key]() mutable {
            try {
                std::vector<uint8_t> payload_bytes;
                std::string encoding = "json";
                
                // 1. Check binary cache first
                bool is_binary = false;
                {
                    std::lock_guard<std::mutex> lock(self->storage_mutex_);
                    auto it = self->local_cache_binary_.find(path);
                    if (it != self->local_cache_binary_.end()) {
                        payload_bytes = it->second.first;
                        encoding = "bytes";
                        is_binary = true;
                    }
                }
                
                // 2. Fall back to JSON cache
                if (!is_binary) {
                    json val;
                    {
                        std::lock_guard<std::mutex> lock(self->storage_mutex_);
                        auto it = self->local_cache_.find(path);
                        if (it != self->local_cache_.end()) {
                            val = it->second;
                        }
                    }
                    std::string val_str = val.dump();
                    payload_bytes = std::vector<uint8_t>(val_str.begin(), val_str.end());
                }

                json resp_header = {
                    {"status", 200},
                    {"metadata", {{"state", "AVAILABLE"}, {"encoding", encoding}}},
                    {"encoding", "json"}
                };
                
                auto* record_bytes = new std::vector<uint8_t>(encode_record(resp_header, payload_bytes));

                z_owned_bytes_t reply_payload;
                z_bytes_from_buf(&reply_payload, (uint8_t*)record_bytes->data(), record_bytes->size(), [](void* data, void* arg) {
                    delete static_cast<std::vector<uint8_t>*>(arg);
                }, record_bytes);

                z_query_reply_options_t options;
                z_query_reply_options_default(&options);

                z_view_keyexpr_t reply_keyexpr;
                z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());

                z_query_reply(z_loan(query_owned), z_loan(reply_keyexpr), z_move(reply_payload), &options);
            } catch (...) {}
            z_drop(z_move(query_owned));
        }).detach();
    }, nullptr, this);

    z_queryable_options_t opts_op;
    z_queryable_options_default(&opts_op);

    z_owned_queryable_t queryable;
    if (z_declare_queryable(z_loan(state->session), &queryable, z_loan(ke_op), z_move(cb_op), &opts_op) == Z_OK) {
        state->queryable_ops.push_back(queryable);
    } else {
        std::cerr << "[VFSNode] Failed to declare queryable for path: " << path << std::endl;
    }
}

} // namespace fs
