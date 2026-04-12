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
        
        auto input_bytes = vfs->read(params.at("source")["path"], params.at("source").value("parameters", nlohmann::json::object()), stack);
        if (input_bytes.empty()) return std::vector<uint8_t>();

        Geometry input_geo;
        input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

        Geometry output_geo = input_geo; // Copy
        applyOutline(output_geo);

        std::string mesh_text = output_geo.encode_text();
        return std::vector<uint8_t>(mesh_text.begin(), mesh_text.end());
    };
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
