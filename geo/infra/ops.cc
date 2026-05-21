#include "../../fs/cpp/vfs_node.h"
#include "processor.h"
#include <iostream>
#include <thread>
#include <cstdlib>

namespace jotcad { namespace geo { void register_all_ops(fs::VFSNode* vfs); } }

using namespace jotcad::geo;

int main(int argc, char** argv) {
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

    fs::VFSNode node(config);
    
    // 1. Register everything from the compiled library onto this specific node instance
    register_all_ops(&node);

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "..." << std::endl;
    node.listen();

    return 0;
}
