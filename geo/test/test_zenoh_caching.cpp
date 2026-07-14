#include <zenoh.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <chrono>
#include <cassert>

// Shared in-memory cache store
std::unordered_map<std::string, std::string> cache_store;
std::mutex cache_mutex;

// Subscriber callback: captures PUT writes and stores them in the cache
void sub_handler(z_loaned_sample_t* sample, void* arg) {
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));

    z_owned_string_t payload_string;
    z_bytes_to_string(z_sample_payload(sample), &payload_string);
    std::string payload(z_string_data(z_loan(payload_string)), z_string_len(z_loan(payload_string)));

    std::cout << "[Subscriber] Captured Put - Key: " << key << " Value: " << payload << std::endl;
    
    std::lock_guard<std::mutex> lock(cache_mutex);
    cache_store[key] = payload;

    z_drop(z_move(payload_string));
}

// Queryable callback: responds from cache if present (Cache Hit), else returns fallback (Cache Miss)
void query_handler(z_loaned_query_t* query, void* context) {
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));

    std::cout << "[Queryable] Query received for key: " << key << std::endl;

    const char* reply_payload_ptr = nullptr;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        auto it = cache_store.find(key);
        if (it != cache_store.end()) {
            std::cout << "[Queryable] CACHE HIT! Returning cached payload." << std::endl;
            // The string is stored persistently in the map, so the c_str() pointer is valid
            reply_payload_ptr = it->second.c_str();
        } else {
            std::cout << "[Queryable] CACHE MISS! Returning default fallback payload." << std::endl;
            reply_payload_ptr = "Default Miss Payload";
        }
    }

    z_query_reply_options_t options;
    z_query_reply_options_default(&options);

    z_owned_bytes_t reply_payload;
    z_bytes_from_static_str(&reply_payload, reply_payload_ptr);

    z_view_keyexpr_t reply_keyexpr;
    z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());

    z_query_reply(query, z_loan(reply_keyexpr), z_move(reply_payload), &options);
}

// Helper function to issue a query and return the payload string
std::string query_vfs(z_owned_session_t& session, const std::string& key) {
    z_view_keyexpr_t q_ke;
    z_view_keyexpr_from_str(&q_ke, key.c_str());

    z_owned_fifo_handler_reply_t handler;
    z_owned_closure_reply_t closure;
    z_fifo_channel_reply_new(&closure, &handler, 16);

    z_get_options_t get_opts;
    z_get_options_default(&get_opts);
    get_opts.timeout_ms = 2000;

    z_get(z_loan(session), z_loan(q_ke), NULL, z_move(closure), &get_opts);

    z_owned_reply_t reply;
    std::string result = "";
    while (z_recv(z_loan(handler), &reply) == Z_OK) {
        if (z_reply_is_ok(z_loan(reply))) {
            const z_loaned_sample_t* sample = z_reply_ok(z_loan(reply));
            z_owned_string_t payload_str;
            z_bytes_to_string(z_sample_payload(sample), &payload_str);
            result = std::string(z_string_data(z_loan(payload_str)), z_string_len(z_loan(payload_str)));
            z_drop(z_move(payload_str));
        }
        z_drop(z_move(reply));
    }
    z_drop(z_move(handler));
    return result;
}

int main() {
    std::cout << "Starting Zenoh caching and cache-clearing test..." << std::endl;

    z_owned_config_t config;
    z_config_default(&config);

    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) != Z_OK) {
        std::cerr << "Unable to open session!" << std::endl;
        return 1;
    }

    z_view_keyexpr_t ke;
    std::string target_expr = "demo/vfs/cache/**";
    z_view_keyexpr_from_str(&ke, target_expr.c_str());

    // 1. Declare Subscriber to intercept PUT writes
    z_owned_closure_sample_t sub_callback;
    z_closure(&sub_callback, sub_handler, NULL, NULL);
    z_owned_subscriber_t sub;
    z_declare_subscriber(z_loan(s), &sub, z_loan(ke), z_move(sub_callback), NULL);

    // 2. Declare Queryable to handle reads
    z_owned_closure_query_t query_callback;
    z_closure(&query_callback, query_handler, NULL, NULL);
    z_owned_queryable_t qable;
    z_queryable_options_t opts;
    z_queryable_options_default(&opts);
    z_declare_queryable(z_loan(s), &qable, z_loan(ke), z_move(query_callback), &opts);

    std::cout << "Cache system ready." << std::endl;

    // --- TEST 1: Cache Miss ---
    std::string query_key = "demo/vfs/cache/abc";
    std::cout << "\n--- Running TEST 1: Initial Cache Miss ---" << std::endl;
    std::string res1 = query_vfs(s, query_key);
    std::cout << "Result 1: " << res1 << std::endl;
    assert(res1 == "Default Miss Payload");

    // --- TEST 2: Cache Write & Cache Hit ---
    std::cout << "\n--- Running TEST 2: Cache Write & Hit ---" << std::endl;
    z_view_keyexpr_t put_ke;
    z_view_keyexpr_from_str(&put_ke, query_key.c_str());
    
    z_owned_bytes_t put_payload;
    std::string cached_value = "Cached Shape Geometry";
    z_bytes_from_static_str(&put_payload, cached_value.c_str());
    
    z_put_options_t put_opts;
    z_put_options_default(&put_opts);
    
    std::cout << "Putting cached value on: " << query_key << std::endl;
    z_put(z_loan(s), z_loan(put_ke), z_move(put_payload), &put_opts);

    // Give subscriber a tiny moment to process the write
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    std::string res2 = query_vfs(s, query_key);
    std::cout << "Result 2: " << res2 << std::endl;
    assert(res2 == cached_value);

    // --- TEST 3: Cache Wiping (Clean Slate) ---
    std::cout << "\n--- Running TEST 3: Cache Wiping / Clean Slate ---" << std::endl;
    {
        std::lock_guard<std::mutex> lock(cache_mutex);
        std::cout << "Wiping local in-memory cache..." << std::endl;
        cache_store.clear();
    }

    std::string res3 = query_vfs(s, query_key);
    std::cout << "Result 3: " << res3 << std::endl;
    assert(res3 == "Default Miss Payload");

    // Graceful teardown
    z_drop(z_move(qable));
    z_drop(z_move(sub));
    z_drop(z_move(s));

    std::cout << "\n✅ Caching and Cache-Clearing Test PASSED!" << std::endl;
    return 0;
}
