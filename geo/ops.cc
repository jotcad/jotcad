#include "../../fs/cpp/include/vfs_node.h"
#include "hexagon_op.h"
#include "box_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "points_op.h"
#include "nth_op.h"
#include "rotate_op.h"
#include "pdf_op.h"
#include "path_op.h"
#include "group_op.h"
#include "impl/processor.h"
#include <iostream>

using namespace jotcad::geo;
using namespace jotcad::fs;

int main(int argc, char** argv) {
    int port = 9091;
    if (argc > 1) port = std::stoi(argv[1]);

    const char* peer_id_env = std::getenv("PEER_ID");
    std::string peer_id = peer_id_env ? peer_id_env : "geo-ops-node";

    VFSNode::Config config;
    config.id = peer_id;
    config.port = port;
    config.version = "1.0.0";
    config.storage_dir = ".vfs_storage_" + peer_id;

    VFSNode node(config);

    std::cout << "[Ops Node] Initializing Geometry Registry..." << std::endl;
    
    // Register all Ops
    box_init();
    hexagon_init();
    triangle_init();
    offset_init();
    outline_init();
    points_init();
    nth_init();
    rotate_init();
    pdf_init();
    path_init();
    group_init();

    // Register all ops from the Processor registry into the VFS Node
    for (auto const& [path, op] : Processor::registry()) {
        std::cout << "[Ops Node] Registering VFS Op: " << path << std::endl;
        node.register_op(path, [path](const VFSNode::VFSRequest& req) {
            return Processor::registry()[path].logic(req.vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }

    // Broadcast batched schema catalog once
    node.notify_schema();

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "... (Build: " << __DATE__ << " " << __TIME__ << ")" << std::endl;
    node.listen();

    return 0;
}
