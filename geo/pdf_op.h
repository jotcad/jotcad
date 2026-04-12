#pragma once
#include "impl/processor.h"
#include "impl/pdf.h"
#include <iostream>

namespace jotcad {
namespace geo {

static std::vector<uint8_t> pdf_op(jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack = {}) {
    std::cout << "[PDF Op] Generating PDF..." << std::endl;
    
    // Read dependency using resolve_geometry
    auto input_bytes = Processor::resolve_geometry(vfs, params.at("source"), stack);
    if (input_bytes.empty()) return {};

    Geometry input_geo;
    input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));

    PDFWriter writer;
    writer.add_geometry(input_geo);
    return writer.write();
}

static void pdf_init() {
    Processor::Operation op;
    op.path = "op/pdf";
    op.logic = pdf_op;
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
