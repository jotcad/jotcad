#include "../../fs/cpp/vfs_node.h"
#include "processor.h"
#include <iostream>
#include <thread>
#include <cstdlib>

namespace jotcad { namespace geo { void register_all_ops(fs::VFSNode* vfs); } }

using namespace jotcad::geo;

int main(int argc, char** argv) {
    int port = 9091;
    if (const char* env_p = std::getenv("PORT")) {
        port = std::stoi(env_p);
    } else if (argc > 1) {
        port = std::stoi(argv[1]);
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
