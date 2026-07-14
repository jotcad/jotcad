#define Z_FEATURE_UNSTABLE_API
#include "vfs.h"
#include <ArduinoOTA.h>

namespace fs {

struct ReadSelectorContext {
    bool done = false;
    bool success = false;
    json result_json;
};

static void reply_dropper(void *ctx) {
    ReadSelectorContext* context = static_cast<ReadSelectorContext*>(ctx);
    context->done = true;
}

static void reply_handler(z_loaned_reply_t *oreply, void *ctx) {
    ReadSelectorContext* context = static_cast<ReadSelectorContext*>(ctx);
    if (z_reply_is_ok(oreply)) {
        const z_loaned_sample_t *sample = z_reply_ok(oreply);
        z_loaned_bytes_t const* payload_bytes = z_sample_payload(sample);
        
        z_view_slice_t view;
        if (z_bytes_get_contiguous_view(payload_bytes, &view) >= 0) {
            const uint8_t* data = view._val.start;
            size_t len = view._val.len;
            
            if (len >= 4) {
                uint32_t header_len = ((uint32_t)data[0] << 24) |
                                      ((uint32_t)data[1] << 16) |
                                      ((uint32_t)data[2] << 8)  |
                                      ((uint32_t)data[3]);
                if (4 + header_len <= len) {
                    std::string header_str((const char*)&data[4], header_len);
                    try {
                        json resp_header = json::parse(header_str);
                        if (resp_header.value("status", 200) == 200) {
                            size_t payload_offset = 4 + header_len;
                            size_t payload_len = len - payload_offset;
                            if (payload_len > 0) {
                                std::string payload_str((const char*)&data[payload_offset], payload_len);
                                try {
                                    context->result_json = json::parse(payload_str);
                                } catch (...) {
                                    context->result_json = json(payload_str);
                                }
                            }
                            context->success = true;
                        }
                    } catch (...) {}
                }
            }
        }
    }
}

VFS::VFS(const Config& config) : config_(config) {}

VFS::~VFS() {
    for (auto& qable : z_queryables_) {
        z_queryable_drop(z_queryable_move(&qable));
    }
    for (auto& pair : z_subscribers_) {
        z_subscriber_drop(z_subscriber_move(&pair.second));
    }
    if (connected_) {
        z_session_drop(z_session_move(&z_session_));
    }
}

void VFS::begin() {
    // 1. Initialize ArduinoOTA for over-the-air firmware updates
    ArduinoOTA.setHostname(config_.id.c_str());
    ArduinoOTA.begin();
    Serial.println("[OTA] Service initialized successfully");

    // 2. Initialize Zenoh Session
    if (config_.neighbors.empty()) {
        Serial.println("[VFS] No neighbors configured. Cannot initialize Zenoh session.");
        return;
    }

    z_owned_config_t config;
    z_config_default(&config);
    zp_config_insert(z_config_loan_mut(&config), Z_CONFIG_MODE_KEY, "client");

    std::string neighbor = config_.neighbors[0];
    std::string connect_endpoint = neighbor;
    if (connect_endpoint.rfind("http://", 0) == 0) {
        connect_endpoint = "tcp/" + connect_endpoint.substr(7);
    } else if (connect_endpoint.rfind("https://", 0) == 0) {
        connect_endpoint = "tcp/" + connect_endpoint.substr(8);
    } else if (connect_endpoint.rfind("tcp/", 0) != 0 && connect_endpoint.rfind("ws/", 0) != 0) {
        connect_endpoint = "tcp/" + connect_endpoint;
    }

    Serial.printf("[VFS %s] Connecting to Zenoh locator: %s\n", config_.id.c_str(), connect_endpoint.c_str());
    zp_config_insert(z_config_loan_mut(&config), Z_CONFIG_CONNECT_KEY, connect_endpoint.c_str());

    // Open Zenoh session
    if (z_open(&z_session_, z_config_move(&config), NULL) < 0) {
        Serial.println("[VFS] CRITICAL: Unable to open Zenoh session!");
        connected_ = false;
        return;
    }
    connected_ = true;
    Serial.println("[VFS] Zenoh session opened successfully.");

    // 3. Declare operator queryables for all registered handlers
    for (const auto& [op_path, handler] : handlers_) {
        std::string key = "jot/vfs/op/" + op_path;
        Serial.printf("[VFS] Declaring operator queryable on: %s\n", key.c_str());

        z_owned_closure_query_t callback;
        z_closure_query(&callback, [](z_loaned_query_t* query, void* ctx) {
            VFS* node = static_cast<VFS*>(ctx);
            node->handle_query(query);
        }, NULL, this);

        z_owned_queryable_t queryable;
        z_view_keyexpr_t ke;
        z_view_keyexpr_from_str_unchecked(&ke, key.c_str());

        if (z_declare_queryable(z_session_loan(&z_session_), &queryable, z_view_keyexpr_loan(&ke), z_closure_query_move(&callback), NULL) < 0) {
            Serial.printf("[VFS] Failed to declare operator queryable for: %s\n", key.c_str());
        } else {
            z_queryables_.push_back(queryable);
        }
    }
}

void VFS::tick() {
    ArduinoOTA.handle();

    if (connected_) {
        if (z_session_is_closed(z_session_loan(&z_session_))) {
            connected_ = false;
        } else {
#if Z_FEATURE_MULTI_THREAD == 0
            zp_spin_once(z_session_loan(&z_session_));

            // Periodically send keep-alive every 2.5 seconds (2500 ms)
            static unsigned long last_keep_alive = 0;
            unsigned long now = millis();
            if (now - last_keep_alive >= 2500) {
                last_keep_alive = now;
                z_result_t res = zp_send_keep_alive(z_session_loan(&z_session_), NULL);
                zp_batch_flush(z_session_loan(&z_session_));
                Serial.printf("[VFS] Keep-alive sent and flushed. Res: %d, Now: %lu\n", res, now);
            }
#endif
        }
    }
}

void VFS::trigger_activity() {
    led_state_ = !led_state_;
}

void VFS::register_op(const std::string& path, Handler handler, const json& schema) {
    handlers_[path] = handler;
    schemas_[path] = schema;
}

void VFS::on_notification(const std::string& path, NotificationHandler handler) {
    if (!connected_) return;

    std::string key = "jot/vfs/pub/" + path;
    Serial.printf("[VFS] Subscribing to notification topic: %s\n", key.c_str());

    auto* ctx_handler = new NotificationHandler(handler);

    z_owned_closure_sample_t callback;
    z_closure_sample(&callback, [](z_loaned_sample_t* sample, void* ctx) {
        NotificationHandler* h = static_cast<NotificationHandler*>(ctx);
        
        z_view_string_t keystr;
        z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
        std::string full_key(z_string_data(z_view_string_loan(&keystr)), z_string_len(z_view_string_loan(&keystr)));
        
        std::string topic_path = (full_key.length() > 12) ? full_key.substr(12) : full_key;
        
        z_owned_string_t payload_str;
        z_bytes_to_string(z_sample_payload(sample), &payload_str);
        std::string raw_msg(z_string_data(z_string_loan(&payload_str)), z_string_len(z_string_loan(&payload_str)));
        z_string_drop(z_string_move(&payload_str));

        try {
            json payload = json::parse(raw_msg);
            (*h)(Selector(topic_path), payload);
        } catch (...) {
            (*h)(Selector(topic_path), json(raw_msg));
        }
    }, [](void* ctx) {
        delete static_cast<NotificationHandler*>(ctx);
    }, ctx_handler);

    z_owned_subscriber_t sub;
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, key.c_str());

    if (z_declare_subscriber(z_session_loan(&z_session_), &sub, z_view_keyexpr_loan(&ke), z_closure_sample_move(&callback), NULL) < 0) {
        Serial.printf("[VFS] Failed to declare subscriber for: %s\n", key.c_str());
        delete ctx_handler;
    } else {
        auto it = z_subscribers_.find(path);
        if (it != z_subscribers_.end()) {
            z_subscriber_drop(z_subscriber_move(&it->second));
        }
        z_subscribers_[path] = sub;
    }
}

void VFS::unlisten(const std::string& path) {
    auto it = z_subscribers_.find(path);
    if (it != z_subscribers_.end()) {
        z_subscriber_drop(z_subscriber_move(&it->second));
        z_subscribers_.erase(it);
        Serial.printf("[VFS] Unlistened from topic: %s\n", path.c_str());
    }
}



json VFS::read_selector(const Selector& sel) {
    std::string key = "jot/vfs/op/" + sel.path;
    
    std::string query_params = "";
    bool first = true;
    for (auto it = sel.parameters.begin(); it != sel.parameters.end(); ++it) {
        if (!first) query_params += ";";
        first = false;
        query_params += it.key() + "=";
        std::string val;
        if (it.value().is_string()) val = it.value().get<std::string>();
        else val = it.value().dump();
        
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
    if (!sel.output.empty()) {
        if (!first) query_params += ";";
        query_params += "output=" + sel.output;
    }
    
    ReadSelectorContext context;
    z_get_options_t opts;
    z_get_options_default(&opts);
    opts.timeout_ms = 5000;
    
    z_owned_closure_reply_t callback;
    z_closure_reply(&callback, reply_handler, reply_dropper, &context);
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, key.c_str());
    
    if (!connected_) return json();
    
    if (z_get(z_session_loan(&z_session_), z_view_keyexpr_loan(&ke), query_params.c_str(), z_closure_reply_move(&callback), &opts) < 0) {
        Serial.println("[VFS] Failed to send z_get query");
        return json();
    }
    
    unsigned long start_ms = millis();
    while (!context.done && millis() - start_ms < 6000) {
#if Z_FEATURE_MULTI_THREAD == 0
        zp_spin_once(z_session_loan(&z_session_));
#endif
        delay(10);
    }
    
    if (context.success) {
        return context.result_json;
    } else {
        Serial.println("[VFS] read_selector failed to get valid reply");
        return json();
    }
}

void VFS::handle_query(z_loaned_query_t* query) {
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_view_string_loan(&key_string)), z_string_len(z_view_string_loan(&key_string)));

    std::string op_path = (key.length() > 11) ? key.substr(11) : key;

    z_view_string_t params_str;
    z_query_parameters(query, &params_str);
    std::string params_raw(z_string_data(z_view_string_loan(&params_str)), z_string_len(z_view_string_loan(&params_str)));

    json params = json::object();
    std::stringstream ss(params_raw);
    std::string item;
    while (std::getline(ss, item, ';')) {
        size_t eq = item.find('=');
        if (eq != std::string::npos) {
            std::string k = item.substr(0, eq);
            std::string val_encoded = item.substr(eq + 1);
            std::string val = "";
            for (size_t i = 0; i < val_encoded.length(); ++i) {
                if (val_encoded[i] == '%') {
                    if (i + 2 < val_encoded.length()) {
                        char hex[3] = { val_encoded[i+1], val_encoded[i+2], 0 };
                        val += (char)std::strtol(hex, nullptr, 16);
                        i += 2;
                    }
                } else if (val_encoded[i] == '+') {
                    val += ' ';
                } else {
                    val += val_encoded[i];
                }
            }

            try {
                params[k] = json::parse(val);
            } catch (...) {
                params[k] = val;
            }
        }
    }

    if (handlers_.count(op_path)) {
        class ZenohResponseWriter : public VFSResponseWriter {
        public:
            ZenohResponseWriter(z_loaned_query_t* q) : query_(q) {}
            void send(int code, const char* contentType, const char* body) override {
                json resp_header = {
                    {"status", code},
                    {"metadata", {{"state", "AVAILABLE"}, {"encoding", "json"}}},
                    {"encoding", "json"}
                };
                
                std::string payload_str = body;
                std::vector<uint8_t> payload_bytes(payload_str.begin(), payload_str.end());
                std::vector<uint8_t> record = encode(resp_header, payload_bytes);
                
                z_owned_bytes_t reply_payload;
                z_bytes_from_buf(&reply_payload, record.data(), record.size(), NULL, NULL);
                z_query_reply(query_, z_query_keyexpr(query_), z_bytes_move(&reply_payload), NULL);
            }
            void send_binary(int code, const char* contentType, const uint8_t* data, size_t len) override {
                json resp_header = {
                    {"status", code},
                    {"metadata", {{"state", "AVAILABLE"}, {"encoding", "bytes"}}},
                    {"encoding", "bytes"}
                };
                
                std::vector<uint8_t> payload_bytes(data, data + len);
                std::vector<uint8_t> record = encode(resp_header, payload_bytes);
                
                z_owned_bytes_t reply_payload;
                z_bytes_from_buf(&reply_payload, record.data(), record.size(), NULL, NULL);
                z_query_reply(query_, z_query_keyexpr(query_), z_bytes_move(&reply_payload), NULL);
            }
        private:
            std::vector<uint8_t> encode(const json& header, const std::vector<uint8_t>& payload) {
                std::string header_str = header.dump();
                uint32_t header_len = header_str.length();
                std::vector<uint8_t> record;
                record.reserve(4 + header_len + payload.size());
                record.push_back((header_len >> 24) & 0xFF);
                record.push_back((header_len >> 16) & 0xFF);
                record.push_back((header_len >> 8) & 0xFF);
                record.push_back(header_len & 0xFF);
                record.insert(record.end(), header_str.begin(), header_str.end());
                record.insert(record.end(), payload.begin(), payload.end());
                return record;
            }
            z_loaned_query_t* query_;
        };

        ZenohResponseWriter writer(query);
        handlers_[op_path](params, &writer);
    }
}

void VFS::publish(const std::string& path, const json& payload) {
    if (!connected_) return;

    local_cache_[path] = payload;

    if (queryables_.find(path) == queryables_.end()) {
        declare_queryable_for_path(path);
    }

    std::string key = "jot/vfs/pub/" + path;
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, key.c_str());

    std::string payload_str = payload.dump();
    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, (const uint8_t*)payload_str.data(), payload_str.size());

    z_put_options_t opts;
    z_put_options_default(&opts);

    z_put(z_session_loan(&z_session_), z_view_keyexpr_loan(&ke), z_bytes_move(&bytes), &opts);
}

void VFS::publish_binary(const std::string& path, const uint8_t* data, size_t len) {
    if (!connected_) return;

    std::vector<uint8_t> vec(data, data + len);
    local_cache_binary_[path] = std::make_pair(vec, "application/octet-stream");

    if (queryables_.find(path) == queryables_.end()) {
        declare_queryable_for_path(path);
    }

    std::string key = "jot/vfs/pub/" + path;
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, key.c_str());

    z_owned_bytes_t bytes;
    z_bytes_copy_from_buf(&bytes, data, len);

    z_put_options_t opts;
    z_put_options_default(&opts);

    z_put(z_session_loan(&z_session_), z_view_keyexpr_loan(&ke), z_bytes_move(&bytes), &opts);
}

void VFS::declare_queryable_for_path(const std::string& path) {
    std::string key = "jot/vfs/op/" + path;
    queryable_keys_.push_back(key);
    const std::string& persistent_key = queryable_keys_.back();

    z_owned_closure_query_t callback;
    z_closure_query(&callback, [](z_loaned_query_t* query, void* ctx) {
        std::pair<VFS*, std::string>* params = static_cast<std::pair<VFS*, std::string>*>(ctx);
        params->first->handle_cached_query(query, params->second);
    }, [](void* ctx) {
        delete static_cast<std::pair<VFS*, std::string>*>(ctx);
    }, new std::pair<VFS*, std::string>(this, path));

    z_owned_queryable_t queryable;
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, persistent_key.c_str());

    if (z_declare_queryable(z_session_loan(&z_session_), &queryable, z_view_keyexpr_loan(&ke), z_closure_query_move(&callback), NULL) >= 0) {
        z_queryables_.push_back(queryable);
        queryables_[path] = true;
    }
}

void VFS::handle_cached_query(z_loaned_query_t* query, const std::string& path) {
    if (local_cache_.find(path) != local_cache_.end()) {
        json val = local_cache_[path];
        json resp_header = {
            {"status", 200},
            {"metadata", {{"state", "AVAILABLE"}, {"encoding", "json"}}},
            {"encoding", "json"}
        };
        std::string body = val.dump();
        std::vector<uint8_t> record;
        std::string header_str = resp_header.dump();
        uint32_t header_len = header_str.length();
        record.reserve(4 + header_len + body.size());
        record.push_back((header_len >> 24) & 0xFF);
        record.push_back((header_len >> 16) & 0xFF);
        record.push_back((header_len >> 8) & 0xFF);
        record.push_back(header_len & 0xFF);
        record.insert(record.end(), header_str.begin(), header_str.end());
        record.insert(record.end(), body.begin(), body.end());

        z_owned_bytes_t reply_payload;
        z_bytes_from_buf(&reply_payload, record.data(), record.size(), NULL, NULL);
        z_query_reply(query, z_query_keyexpr(query), z_bytes_move(&reply_payload), NULL);
    }
    else if (local_cache_binary_.find(path) != local_cache_binary_.end()) {
        const auto& [data, mime] = local_cache_binary_[path];
        json resp_header = {
            {"status", 200},
            {"metadata", {{"state", "AVAILABLE"}, {"encoding", "bytes"}}},
            {"encoding", "bytes"}
        };
        std::vector<uint8_t> record;
        std::string header_str = resp_header.dump();
        uint32_t header_len = header_str.length();
        record.reserve(4 + header_len + data.size());
        record.push_back((header_len >> 24) & 0xFF);
        record.push_back((header_len >> 16) & 0xFF);
        record.push_back((header_len >> 8) & 0xFF);
        record.push_back(header_len & 0xFF);
        record.insert(record.end(), header_str.begin(), header_str.end());
        record.insert(record.end(), data.begin(), data.end());

        z_owned_bytes_t reply_payload;
        z_bytes_from_buf(&reply_payload, record.data(), record.size(), NULL, NULL);
        z_query_reply(query, z_query_keyexpr(query), z_bytes_move(&reply_payload), NULL);
    }
}

} // namespace fs
