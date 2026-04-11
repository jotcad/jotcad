#pragma once
#include "impl/processor.h"
#include "impl/offset.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> offset_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[Offset Op] Offsetting mesh..." << std::endl;
    double radius = params.value("radius", 1.0);
    
    // Read dependency
    auto input_bytes = vfs->read(params.at("source")["path"], params.at("source").value("parameters", nlohmann::json::object()), stack);
    if (input_bytes.empty()) return {};

    Geometry input_geo;
    input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

    // Apply transformation
    applyOffset(input_geo, radius);

    // Write result
    std::string mesh_text = input_geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "offset"}, {"radius", radius}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void offset_init() {
    Processor::Operation op;
    op.path = "op/offset";
    op.logic = offset_op;
    op.schema = {
        {"type", "object"},
        {"properties", {
            {"source", {{"type", "string"}}},
            {"radius", {{"type", "number"}, {"default", 1}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
