#include "../../fs/cpp/include/vfs_node.h"
#include "impl/processor.h"
#include <iostream>
#include <thread>

namespace jotcad { namespace geo { void register_all_ops(); } }

using namespace jotcad::geo;

int main(int argc, char** argv) {
    int port = (argc > 1) ? std::stoi(argv[1]) : 9091;
    fs::VFSNode::Config config;
    config.id = "geo-ops-node";
    config.port = port;
    config.storage_dir = ".vfs_storage_geo-ops-node";

    fs::VFSNode node(config);
    
    // 1. Register everything from the compiled library
    register_all_ops();

    // 2. Map Processor Registry to VFS Node
    for (auto const& [path, op] : Processor::registry_instance()) {
        node.register_op(path, [&node, path](const fs::VFSNode::VFSRequest& req) {
            return Processor::registry_instance()[path].handler(&node, req);
        }, op.schema);
    }

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "..." << std::endl;
    node.listen();

    return 0;
}
