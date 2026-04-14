#include "impl/processor.h"
#include "impl/node_shim.h"
#include "hexagon_op.h"
#include "box_op.h"
#include "triangle_op.h"
#include "offset_op.h"
#include "outline_op.h"
#include "pdf_op.h"
#include "grammar_ops.h"
#include <iostream>
#include <cstdlib>

using namespace jotcad::geo;
using namespace jotcad::fs;

int main(int argc, char** argv) {
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
    config.port = port;
    config.storage_dir = ".vfs_storage_" + peer_id;
    config.neighbors = neighbors;

    VFSNode node(config);
    NodeClientShim shim(node);

    std::cout << "[Ops Node] Initializing Geometry Registry..." << std::endl;

    // Fundamental Shapes
    hexagon_init();
    box_init();
    triangle_init();
    
    // Geometric Grammar
    grammar_init();

    // Registry for a simple origin point
    Processor::Operation origin_op;
    origin_op.path = "shape/origin";
    origin_op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::vector<uint8_t> geo = {'v', ' ', '0', '.', '0', '0', '0', '0', '0', '0', ' ', '0', '.', '0', '0', '0', '0', '0', '0', ' ', '0', '.', '0', '0', '0', '0', '0', '0', '\n'};
        
        vfs->write("geo/mesh", params, geo);
        nlohmann::json shape = {
            {"geometry", "vfs:/geo/mesh"},
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
        node.register_op(path, [&shim, op, path](const VFSNode::VFSRequest& req) {
            return op.logic(&shim, req.path, req.parameters, req.stack);
        }, op.schema);
    }

    std::cout << "[Ops Node] Starting Native VFS Node on port " << port << "... (Build: " << __DATE__ << " " << __TIME__ << ")" << std::endl;
    node.listen();

    return 0;
}
