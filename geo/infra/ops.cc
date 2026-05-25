#include "../../fs/cpp/vfs_node.h"
#include "processor.h"
#include <iostream>
#include <thread>
#include <cstdlib>
#include <csignal>

namespace jotcad { namespace geo { void register_all_ops(fs::VFSNode* vfs); } }

using namespace jotcad::geo;

static fs::VFSNode* global_node = nullptr;

void signal_handler(int signal) {
    if (global_node) {
        std::cout << "\n[Ops Node] Signal " << signal << " received. Stopping server..." << std::endl;
        global_node->stop();
    }
}

int main(int argc, char** argv) {
    // ... (rest of help/schema checks unchanged)
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            std::cout << "Usage: ops [port] [storage_dir]\n"
                      << "       ops --schema-only\n"
                      << "Environment variables: PORT\n";
            return 0;
        }
        if (arg1 == "--schema-only") {
            fs::VFSNode::Config config;
            config.id = "temp-schema-node";
            fs::VFSNode node(config);
            register_all_ops(&node);
            std::cout << node.get_catalog().dump(2) << std::endl;
            return 0;
        }
    }

    int port = 9091;
    // ... (port/config logic unchanged)
    if (const char* env_p = std::getenv("PORT")) {
        port = std::stoi(env_p);
    } else if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port: " << argv[1] << std::endl;
            return 1;
        }
    }

    fs::VFSNode::Config config;
    config.id = "geo-ops-node";
    config.port = port;
    config.storage_dir = (argc > 2) ? argv[2] : ".vfs_storage_geo-ops-node";

    if (const char* cert_p = std::getenv("SSL_CERT_PATH")) config.cert_path = cert_p;
    if (const char* key_p = std::getenv("SSL_KEY_PATH")) config.key_path = key_p;

    fs::VFSNode node(config);
    global_node = &node;
    
    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 1. Register everything from the compiled library onto this specific node instance
    register_all_ops(&node);

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "..." << std::endl;
    node.listen();
    
    global_node = nullptr; // Safety: node is about to go out of scope

    return 0;
}
