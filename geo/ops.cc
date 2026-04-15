#include "../../fs/cpp/include/vfs_node.h"
#include "hexagon_op.h"
#include "box_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "points_op.h"
#include "nth_op.h"
#include "group_op.h"
#include "path_op.h"
#include "pdf_op.h"
#include <iostream>
#include <cstdlib>

using namespace jotcad::geo;
using namespace jotcad::fs;

int main(int argc, char** argv) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::cout << "JotCAD Ops Node v1.1.0 (Build: " << __DATE__ << " " << __TIME__ << ")" << std::endl;
            return 0;
        }
    }

    std::string peer_id = std::getenv("PEER_ID") ? std::getenv("PEER_ID") : "geo-ops-node";
    int port = std::getenv("PORT") ? std::atoi(std::getenv("PORT")) : 9091;
    std::string neighbors_str = std::getenv("NEIGHBORS") ? std::getenv("NEIGHBORS") : "";
    
    std::vector<std::string> neighbors;
    size_t pos = 0;
    while ((pos = neighbors_str.find(',')) != std::string::npos) {
        neighbors.push_back(neighbors_str.substr(0, pos));
        neighbors_str.erase(0, pos + 1);
    }
    if (!neighbors_str.empty()) neighbors.push_back(neighbors_str);

    VFSNode::Config config;
    config.id = peer_id;
    config.version = "JotCAD Ops Node v1.1.0 (Build: " + std::string(__DATE__) + " " + std::string(__TIME__) + ")";
    config.port = port;
    config.storage_dir = ".vfs_storage_" + peer_id;
    config.neighbors = neighbors;

    VFSNode node(config);

    std::cout << "[Ops Node] Initializing Geometry Registry..." << std::endl;

    // Fundamental Shapes
    hexagon_init();
    box_init();
    triangle_init();
    
    // Geometric Grammar (Separated Ops)
    points_init();
    nth_init();
    group_init();
    path_init();

    // Registry for a simple origin point
    Processor::Operation origin_op;
    origin_op.path = "jot/Origin";
    origin_op.logic = [](VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::vector<uint8_t> geo = {'v', ' ', '0', '.', '0', '0', '0', '0', '0', '0', ' ', '0', '.', '0', '0', '0', '0', '0', '0', ' ', '0', '.', '0', '0', '0', '0', '0', '0', '\n'};
        
        std::string hash = vfs->write_cid("geo/mesh", geo);
        nlohmann::json shape = {
            {"geometry", {
                {"path", "geo/mesh"},
                {"parameters", {{"cid", hash}}}
            }},
            {"parameters", params},
            {"tags", {{"type", "origin"}}}
        };
        std::string shape_text = shape.dump();
        return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
    };
    origin_op.schema = {
        {"arguments", {}},
        {"inputs", {}},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(origin_op);

    // Transformations & Exports
    offset_init();
    outline_init();
    pdf_init();

    // REGISTER ALL OPS from Processor to VFSNode
    for (auto const& [path, op] : Processor::registry()) {
        std::cout << "[Ops Node] Registering VFS Op: " << path << std::endl;
        node.register_op(path, [&node, op](const VFSNode::VFSRequest& req) {
            return op.logic(&node, req.path, req.parameters, req.stack);
        }, op.schema);
    }

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "... (Build: " << __DATE__ << " " << __TIME__ << ")" << std::endl;
    node.listen();

    return 0;
}
