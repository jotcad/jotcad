#pragma once
#include "protocols.h"
#include "processor.h"
#include "pdf.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PdfOp : P {
    static constexpr const char* path = "jot/pdf";

    static void walk(fs::VFSNode* vfs, const Shape& shape, PDFWriter& writer) {
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                geo.apply_tf(shape.tf);
                writer.add_geometry(geo);
            } catch (const std::exception& e) {
                std::cerr << "[PdfOp::walk] Error reading geometry: " << e.what() << std::endl;
            }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, writer);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& pdf_path, double width, double height) {
        PDFConfig config;
        config.page_width = width;
        config.page_height = height;
        
        PDFWriter writer(config);
        walk(vfs, in, writer);
        auto pdf_bytes = writer.write();
        
        // Output: PDF bytes in the primary '$out' port
        vfs->write(fulfilling.with_output("$out"), pdf_bytes);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path", "width", "height"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/pdf"},
            {"description", "Generates a PDF document from the spatial representation of the input shape."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}},
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.pdf"}},
                {{"name", "width"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Page width in mm (0 = auto)"}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Page height in mm (0 = auto)"}}
            }},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "application/pdf"}, {"description", "The generated PDF blob."}}}
            }}
        };
    }
};

static void pdf_init(fs::VFSNode* vfs) {
    Processor::register_op<PdfOp<>, Shape, std::string, double, double>(vfs, "jot/pdf");
}

} // namespace geo
} // namespace jotcad
