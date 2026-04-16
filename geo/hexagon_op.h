#pragma once
#include "impl/processor.h"
#include "impl/hexagon.h"
#include "../../fs/cpp/include/vfs_node.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> hexagon_op_internal(jotcad::fs::VFSNode* vfs, double radius, const std::string& variant, const nlohmann::json& params) {
    Geometry geo;
    makeHexagon(geo, radius, variant);
    
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
        {"tags", {{"type", "hexagon", {"variant", variant}}}}
    };
    std::string shape_text = shape.dump();
    return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
}

static void hexagon_init() {
    auto register_variant = [](const std::string& variant) {
        Processor::Operation op;
        op.path = "jot/Hexagon/" + variant;
        op.logic = [variant](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
            double radius = 0;
            if (params.contains("diameter")) {
                radius = params.at("diameter").get<double>() / 2.0;
            } else if (params.contains("radius")) {
                radius = params.at("radius").get<double>();
            } else if (params.contains("apothem")) {
                // d = 2 * a / cos(30) => r = a / cos(30)
                radius = params.at("apothem").get<double>() / 0.86602540378;
            }
            return hexagon_op_internal(vfs, radius, variant, params);
        };
        op.schema = {
            {"arguments", {
                {"diameter", {{"type", "number"}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "shape"}}}
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
