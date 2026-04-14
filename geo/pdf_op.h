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
        
        auto in_selector = params.at("$in");
        auto input_bytes = vfs->read(in_selector["path"], in_selector.value("parameters", nlohmann::json::object()), stack);
        
        if (input_bytes.empty()) return std::vector<uint8_t>();

        // 1. Perform the side effect (Generate PDF)
        try {
            Geometry input_geo;
            input_geo.decode_text(std::string(input_bytes.begin(), input_bytes.end()));
            PDFWriter writer;
            writer.add_geometry(input_geo);
            auto pdf_data = writer.write();
            
            // The 'path' argument overlaps with the 'path' output sink
            std::string target_path = params.value("path", "export.pdf");
            vfs->write(target_path, {}, pdf_data);
            std::cout << "[PDF Op] PDF Saved to " << target_path << " (" << pdf_data.size() << " bytes)." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[PDF Op] Error generating PDF: " << e.what() << std::endl;
        }

        // 2. Pass-through: Return the original geometry for the $out port
        return input_bytes;
    };
    op.schema = {
        {"arguments", {
            {"$in", {{"type", "shape"}, {"description", "Input geometry"}}},
            {"$out", {{"type", "shape"}, {"description", "Functional return (geometry)"}}},
            {"path", {{"type", "string"}, {"format", "vfs-path"}, {"default", "export.pdf"}}}
        }},
        {"inputs", {
            {"$in", {{"type", "shape"}}}
        }},
        {"outputs", {
            {"$out", {{"type", "shape"}}},
            {"path", {{"mime", "application/pdf"}}}
        }},
        {"metadata", {
            {"aliases", {{"$out", "$in"}}}
        }}
    };
    Processor::register_op(op);
}

} // namespace geo
} // namespace jotcad
