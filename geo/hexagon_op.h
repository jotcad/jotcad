#pragma once
#include "impl/processor.h"
#include "impl/hexagon.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> hexagon_op_internal(jotcad::fs::VFSClient* vfs, double radius, const std::string& variant, const nlohmann::json& params) {
    Geometry geo;
    makeHexagon(geo, radius, variant);

    // Write the raw mesh
    std::string mesh_text = geo.encode_text();
    std::vector<uint8_t> mesh_data(mesh_text.begin(), mesh_text.end());
    vfs->write("geo/mesh", params, mesh_data);

    // Return the Shape JSON
    nlohmann::json shape = {
        {"geometry", "vfs:/geo/mesh"},
        {"parameters", params},
        {"tags", {{"type", "hexagon"}, {"variant", variant}}}
    };
    std::string s = shape.dump();
    return std::vector<uint8_t>(s.begin(), s.end());
}

static void hexagon_init() {
    auto register_variant = [](const std::string& variant) {
        Processor::Operation op;
        op.path = "shape/hexagon/" + variant;
        op.logic = [variant](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double radius = params.at("radius").get<double>();
            return hexagon_op_internal(vfs, radius, variant, params);
        };
        op.schema = {
            {"type", "object"},
            {"required", {"radius"}},
            {"properties", {
                {"radius", {{"type", "number"}}}
            }}
        };
        Processor::register_op(op);
    };

    register_variant("full");
    register_variant("half");
    register_variant("middle");
    register_variant("cap");
    register_variant("sector");
}

} // namespace geo
} // namespace jotcad
