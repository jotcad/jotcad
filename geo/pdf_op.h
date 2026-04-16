#pragma once
#include "impl/processor.h"
#include "impl/pdf.h"
#include <iostream>

namespace jotcad {
namespace geo {

static void pdf_init() {
    Processor::Operation op;
    op.path = "jot/pdf";
    op.logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        std::cout << "[PDF Op] Generating PDF (Passive)..." << std::endl;
        
        auto in_selector = params.at("$in");
        jotcad::fs::VFSNode::VFSRequest in_req;
        in_req.path = in_selector["path"];
        in_req.parameters = in_selector.value("parameters", nlohmann::json::object());
        in_req.stack = stack;
        auto shape_bytes = vfs->read(in_req);
        if (shape_bytes.empty()) return std::vector<uint8_t>();

        // 1. Parse Input Shape
        nlohmann::json in_shape = nlohmann::json::parse(std::string(shape_bytes.begin(), shape_bytes.end()));
        
        // 2. Resolve Underlying Geometry
        auto geo_selector = in_shape.at("geometry");
        jotcad::fs::VFSNode::VFSRequest geo_req;
        geo_req.path = geo_selector["path"];
        geo_req.parameters = geo_selector.value("parameters", nlohmann::json::object());
        geo_req.stack = stack;
        auto geo_bytes = vfs->read(geo_req);
        if (geo_bytes.empty()) return std::vector<uint8_t>();

        // 3. Perform the side effect (Generate PDF)
        try {
            Geometry input_geo;
            input_geo.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));

            if (in_shape.contains("tf")) {
                input_geo.apply_tf(in_shape.at("tf").get<std::vector<double>>());
            }

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

        // 4. Pass-through: Return the original SHAPE (not geometry)
        return shape_bytes;
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
            {"$out", {{"type", "shape"}, {"description", "Functional return (geometry)"}}},
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
