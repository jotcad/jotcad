#pragma once
#include "impl/processor.h"
#include "impl/pdf.h"
#include <iostream>

namespace jotcad {
namespace geo {

static void pdf_init() {
    Processor::Operation op;
    op.path = "op/pdf";
    op.logic = [](jotcad::fs::VFSClient* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[PDF Op] Generating PDF (Passive)..." << std::endl;
        
        auto source_selector = params.at("source");
        auto input_bytes = vfs->read(source_selector["path"], source_selector.value("parameters", nlohmann::json::object()), stack);
        
        if (input_bytes.empty()) return std::vector<uint8_t>();

        // 1. Perform the side effect (Generate PDF)
        // In a real implementation, this might write to a local disk or a 'vfs:/exports/...' path.
        try {
            Geometry input_geo;
            input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));
            PDFWriter writer;
            writer.add_geometry(input_geo);
            auto pdf_data = writer.write();
            
            // For now, we simulate writing to a known export path
            // vfs->write("exports/last_generation.pdf", {}, pdf_data);
            std::cout << "[PDF Op] PDF Generated: " << pdf_data.size() << " bytes." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PDF Op] Error generating PDF: " << e.what() << std::endl;
        }

        // 2. Pass-through: Return the original geometry
        return input_bytes;
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
