#pragma once
#include "impl/processor.h"
#include "impl/offset.h"
#include <iostream>

namespace jotcad {
namespace geo {

static void offset_init() {
    Processor::Operation op;
    op.path = "op/offset";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Offset Op] Offsetting mesh..." << std::endl;
        double radius = params.value("radius", 1.0);
        
        auto in_selector = params.at("$in");
        auto input_bytes = vfs->read(in_selector["path"], in_selector.value("parameters", nlohmann::json::object()), stack);
        
        if (input_bytes.empty()) return std::vector<uint8_t>();

        Geometry input_geo;
        input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

        Geometry output_geo = input_geo; // Start with a copy
        applyOffset(output_geo, radius);

        std::string mesh_text = output_geo.encode_text();
        return std::vector<uint8_t>(mesh_text.begin(), mesh_text.end());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}, {"description", "Input geometry"}}},
            {"$out", {{"type", "shape"}, {"description", "Resulting geometry"}}},
            {"radius", {{"type", "number"}, {"default", 1}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
