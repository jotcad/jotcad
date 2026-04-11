#pragma once
#include "impl/processor.h"
#include "impl/box.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> box_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[Box Op] Generating box with params: " << params.dump() << std::endl;
    double width = params.value("width", 10.0);
    double height = params.value("height", 10.0);
    double depth = params.value("depth", 10.0);

    Geometry geo;
    makeBox(geo, width, height, depth);

    // 1. Write the raw mesh to a content-addressed location (geo/mesh)
    std::string mesh_text = geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    // 2. Return the Shape JSON for the requested path
    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "box"}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void box_init() {
    Processor::Operation op;
    op.path = "shape/box";
    op.logic = box_op;
    op.schema = {
        {"type", "object"},
        {"properties", {
            {"width", {{"type", "number"}, {"default", 10}}},
            {"height", {{"type", "number"}, {"default", 10}}},
            {"depth", {{"type", "number"}, {"default", 10}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
