#pragma once
#include "impl/processor.h"
#include "impl/outline.h"
#include <iostream>

namespace jotcad {
namespace geo {

static void outline_init() {
    Processor::Operation op;
    op.path = "op/outline";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[Outline Op] Outlining mesh..." << std::endl;
        
        auto in_selector = params.at("$in");
        auto input_bytes = vfs->read(in_selector["path"], in_selector.value("parameters", nlohmann::json::object()), stack);
        if (input_bytes.empty()) return std::vector<uint8_t>();

        Geometry input_geo;
        input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

        Geometry output_geo = input_geo;
        applyOutline(output_geo);

        // 1. Write raw mesh to content-addressed store
        std::string mesh_text = output_geo.encode_text();
        vfs->write("geo/mesh", params, std::vector<uint8_t>(mesh_text.begin(), mesh_text.end()));

        // 2. Return Shape JSON
        nlohmann::json shape = {
            {"geometry", "vfs:/geo/mesh"},
            {"parameters", params},
            {"tags", {{"type", "outline"}}}
        };
        std::string shape_text = shape.dump();
        return std::vector<uint8_t>(shape_text.begin(), shape_text.end());
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}}},
            {"$out", {{"type", "shape"}}}
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
