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

    static void walk(fs::VFSNode* vfs, const Shape& shape, const Matrix& parent_tf, PDFWriter& writer) {
        Matrix current_tf = parent_tf * shape.tf;
        
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                geo.apply_tf(current_tf);
                writer.add_geometry(geo);
            } catch (const std::exception& e) {
                std::cerr << "[PdfOp::walk] Error reading geometry: " << e.what() << std::endl;
            }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, current_tf, writer);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& pdf_path, double width, double height) {
        PDFConfig config;
        config.page_width = width;
        config.page_height = height;
        
        PDFWriter writer(config);
        walk(vfs, in, Matrix::identity(), writer);
        auto pdf_bytes = writer.write();
        
        // 1. Primary Output: Shape Pass-through
        vfs->write(fulfilling.with_output("$out"), in);

        // 2. Secondary Output: PDF bytes in 'file' port
        vfs->write(fulfilling.with_output("file"), pdf_bytes);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path", "width", "height"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/pdf"},
            {"description", "Generates a PDF document from the spatial representation of the input shape."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.pdf"}},
                {{"name", "width"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Page width in mm (0 = auto)"}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 0.0}, {"description", "Page height in mm (0 = auto)"}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The input shape (pass-through)."}}},
                {"file", {{"type", "file"}, {"mimeType", "application/pdf"}, {"description", "The generated PDF blob."}}}
            }}
        };
    }
};

static void pdf_init(fs::VFSNode* vfs) {
    Processor::register_op<PdfOp<>, Shape, std::string, double, double>(vfs, "jot/pdf");
}

} // namespace geo
} // namespace jotcad
