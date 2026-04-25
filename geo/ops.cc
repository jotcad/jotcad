#include "../../fs/cpp/vfs_node.h"
#include "impl/processor.h"
#include <iostream>
#include <thread>

namespace jotcad { namespace geo { void register_all_ops(fs::VFSNode* vfs); } }

using namespace jotcad::geo;

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 9091;
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
