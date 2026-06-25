#include <zenoh.h>
#include <iostream>
#include <string>
#include <cassert>

void query_handler(z_loaned_query_t *query, void *context) {
    std::cout << "[Queryable callback] Triggered!" << std::endl;
    
    z_view_string_t key_string;
    z_keyexpr_as_view_string(z_query_keyexpr(query), &key_string);
    std::string key(z_string_data(z_loan(key_string)), z_string_len(z_loan(key_string)));
    
    z_view_string_t params_str;
    z_query_parameters(query, &params_str);
    std::string params(z_string_data(z_loan(params_str)), z_string_len(z_loan(params_str)));

    std::cout << "[Queryable callback] Key: " << key << " Params: " << params << std::endl;

    // Use a static literal string so the pointer remains valid after callback returns
    z_query_reply_options_t options;
    z_query_reply_options_default(&options);

    z_owned_bytes_t reply_payload;
    z_bytes_from_static_str(&reply_payload, "Fulfillment Success!");

    z_view_keyexpr_t reply_keyexpr;
    z_view_keyexpr_from_str(&reply_keyexpr, key.c_str());

    std::cout << "[Queryable callback] Responding: Fulfillment Success!" << std::endl;
    z_query_reply(query, z_loan(reply_keyexpr), z_move(reply_payload), &options);
}

int main() {
    std::cout << "Starting Zenoh queryable test..." << std::endl;
    
    z_owned_config_t config;
    z_config_default(&config);
    
    z_owned_session_t s;
    if (z_open(&s, z_move(config), NULL) != Z_OK) {
        std::cerr << "Unable to open session!" << std::endl;
        return 1;
    }
    std::cout << "Session opened successfully." << std::endl;

    // Register queryable on demo/vfs/**
    z_view_keyexpr_t ke;
    std::string target_expr = "demo/vfs/**";
    if (z_view_keyexpr_from_str(&ke, target_expr.c_str()) < 0) {
        std::cerr << "Invalid key expression: " << target_expr << std::endl;
        return 1;
    }

    z_owned_closure_query_t callback;
    z_closure(&callback, query_handler, NULL, NULL);
    
    z_owned_queryable_t qable;
    z_queryable_options_t opts;
    z_queryable_options_default(&opts);

    if (z_declare_queryable(z_loan(s), &qable, z_loan(ke), z_move(callback), &opts) != Z_OK) {
        std::cerr << "Unable to declare queryable!" << std::endl;
        return 1;
    }
    std::cout << "Queryable declared on: " << target_expr << std::endl;

    // Issue query: demo/vfs/selector/abc?dx=0.5
    std::string query_key = "demo/vfs/selector/abc";
    std::string query_params = "dx=0.5";
    
    z_view_keyexpr_t q_ke;
    z_view_keyexpr_from_str(&q_ke, query_key.c_str());

    z_owned_fifo_handler_reply_t handler;
    z_owned_closure_reply_t closure;
    z_fifo_channel_reply_new(&closure, &handler, 16);

    z_get_options_t get_opts;
    z_get_options_default(&get_opts);
    get_opts.timeout_ms = 5000;

    std::cout << "Sending query: " << query_key << "?" << query_params << "..." << std::endl;
    z_get(z_loan(s), z_loan(q_ke), query_params.c_str(), z_move(closure), &get_opts);

    z_owned_reply_t reply;
    bool success = false;
    while (z_recv(z_loan(handler), &reply) == Z_OK) {
        if (z_reply_is_ok(z_loan(reply))) {
            const z_loaned_sample_t* sample = z_reply_ok(z_loan(reply));
            
            z_view_string_t reply_key_str;
            z_keyexpr_as_view_string(z_sample_keyexpr(sample), &reply_key_str);
            std::string reply_key(z_string_data(z_loan(reply_key_str)), z_string_len(z_loan(reply_key_str)));

            z_owned_string_t payload_str;
            z_bytes_to_string(z_sample_payload(sample), &payload_str);
            std::string payload(z_string_data(z_loan(payload_str)), z_string_len(z_loan(payload_str)));

            std::cout << ">> Received reply [Key: " << reply_key << "] -> Payload: " << payload << std::endl;
            
            assert(payload == "Fulfillment Success!");
            assert(reply_key == query_key);
            success = true;
            
            z_drop(z_move(payload_str));
        } else {
            std::cerr << "Received reply error!" << std::endl;
        }
        z_drop(z_move(reply));
    }

    z_drop(z_move(handler));
    z_drop(z_move(qable));
    z_drop(z_move(s));

    if (success) {
        std::cout << "✅ Fulfillment on Cache Miss Test PASSED!" << std::endl;
        return 0;
    } else {
        std::cerr << "❌ Fulfillment Test Failed (No response received)!" << std::endl;
        return 1;
    }
}
