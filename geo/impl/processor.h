#pragma once

#include "../../fs/cpp/include/vfs_client.h"
#include "geometry.h"
#include <string>
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <functional>

namespace jotcad {
namespace geo {

class Processor {
public:
    using LogicHandler = std::function<std::vector<uint8_t>(jotcad::fs::VFSClient*, const std::string&, const nlohmann::json&)>;
    
    struct Operation {
        std::string path;
        LogicHandler logic;
        nlohmann::json schema;
        std::string ux_code;
        std::string ux_path; // e.g. "ui/components/box"
    };

    static std::map<std::string, Operation>& registry() {
        static std::map<std::string, Operation> reg;
        return reg;
    }

    static void register_op(const Operation& op) {
        std::cout << "[Processor] Registering operation: " << op.path << std::endl;
        registry()[op.path] = op;
    }

    struct Context {
        std::string hub_url;
        std::string peer_id;
    };

    static Context parse_args(int argc, char** argv) {
        Context ctx;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--hub" && i + 1 < argc) ctx.hub_url = argv[++i];
            else if (arg == "--peer" && i + 1 < argc) ctx.peer_id = argv[++i];
        }
        return ctx;
    }

    static int serve(int argc, char** argv) {
        auto ctx = parse_args(argc, argv);
        if (ctx.hub_url.empty()) {
            std::cerr << "Usage: processor --hub <url> [--peer <id>]" << std::endl;
            return 1;
        }

        std::string peer_id = ctx.peer_id.empty() ? "geo-service" : ctx.peer_id;
        std::cout << "[Processor] Service starting as peer: " << peer_id << std::endl;
        std::cout << "[Processor] Connecting to Hub at: " << ctx.hub_url << std::endl;
        auto vfs = jotcad::fs::create_rest_client(ctx.hub_url, peer_id);

// Give the Hub a moment to stabilize if it was just started
std::this_thread::sleep_for(std::chrono::seconds(1));

// Register the mailbox handler
vfs->on_read([&](const std::string& path, const nlohmann::json& params) {
    auto it = registry().find(path);
    if (it != registry().end()) {
        std::cout << "[Processor] Dispatching READ request to operation: " << path << std::endl;
        try {
            return it->second.logic(vfs.get(), path, params);
        } catch (const std::exception& e) {
            std::cerr << "[Processor] Logic Error: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "[Processor] Error: Unknown path in mailbox: " << path << std::endl;
    }
    return std::vector<uint8_t>();
});

// Start watching (starts background SSE thread)
vfs->watch("*", [](const jotcad::fs::VFSEvent& ev) {
    // Internal events handled by VFS client
});

        // Initialize and Advertise all registered operations
        std::cout << "[Processor] Initializing " << registry().size() << " registered operations..." << std::endl;
        for (auto& [path, op] : registry()) {
            try {
                std::cout << "[Processor]   -> " << path << std::endl;

                // 1. Declare Schema
                if (!op.schema.empty()) {
                    vfs->declare(path, op.schema);
                }

                // 2. Seed UX code if provided
                if (!op.ux_code.empty() && !op.ux_path.empty()) {
                    std::vector<uint8_t> ux_data(op.ux_code.begin(), op.ux_code.end());
                    vfs->write(op.ux_path, nlohmann::json::object(), ux_data);
                }

                // 3. Advertise LISTENING
                vfs->write_state(path, nlohmann::json::object(), "LISTENING", "peer:" + peer_id);
            } catch (const std::exception& e) {
                std::cerr << "[Processor] FATAL error during initialization of " << path << ": " << e.what() << std::endl;
                // We continue to the next op instead of crashing the whole service
            }
        }
        std::cout << "[Processor] Service ready. Waiting for requests..." << std::endl;

        // Block main thread with heartbeat
        while (true) {
            std::cout << "[Processor] Heartbeat: " << peer_id << " is alive (" << registry().size() << " operations registered)" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }

        return 0;
    }
};

} // namespace geo
} // namespace jotcad
