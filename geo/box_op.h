#pragma once
#include "impl/processor.h"
#include "impl/box.h"
#include "../../fs/cpp/include/vfs_node.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> box_op(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[Box Op] Generating box with params: " << params.dump() << std::endl;
    
    double dflt = params.value("size", 10.0);
    double width = params.value("width", dflt);
    double height = params.value("height", dflt);
    double depth = params.value("depth", dflt);

    Geometry geo;
    makeBox(geo, width, height, depth);

    // 1. Write the raw mesh to a content-addressed location (geo/mesh)
    std::string mesh_text = geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    std::string hash = vfs->write_cid("geo/mesh", mesh_data);

    // 2. Return the Shape JSON referencing the CID via a structured selector
    nlohmann::json shape = {
        {"geometry", {
            {"path", "geo/mesh"},
            {"parameters", {{"cid", hash}}}
        }},
        {"parameters", params},
        {"tags", {{"type", "box"}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void box_init() {
    Processor::Operation op;
    op.path = "jot/Box";
    op.logic = box_op;
    op.schema = {
        {"arguments", {
            {"width", {{"type", "number"}, {"default", 10}}},
            {"height", {{"type", "number"}, {"default", 10}}},
            {"depth", {{"type", "number"}, {"default", 10}}}
        }},
        {"inputs", {}},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
