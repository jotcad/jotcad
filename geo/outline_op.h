#pragma once
#include "impl/processor.h"
#include "impl/outline.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> outline_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[Outline Op] Outlining mesh..." << std::endl;
    
    // Read dependency using resolve_geometry
    auto input_bytes = Processor::resolve_geometry(vfs, params.at("source"), stack);
    if (input_bytes.empty()) return {};

    Geometry input_geo;
    input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

    // Apply transformation
    applyOutline(input_geo);

    // Write result
    std::string mesh_text = input_geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "outline"}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void outline_init() {
    Processor::Operation op;
    op.path = "op/outline";
    op.logic = outline_op;
    op.schema = {
        {"type", "object"},
        {"properties", {
            {"source", {{"type", "object"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
