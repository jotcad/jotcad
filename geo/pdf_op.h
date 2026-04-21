#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/pdf.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PdfOp : P {
    static constexpr const char* path = "jot/pdf";

    static void walk(fs::VFSNode* vfs, const Shape& shape, const Matrix& parent_tf, PDFWriter& writer) {
        Matrix current_tf = parent_tf * Matrix::from_vec(shape.tf);
        
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                geo.apply_tf(current_tf.to_vec());
                writer.add_geometry(geo);
            } catch (...) { }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, current_tf, writer);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& pdf_path) {
        std::cout << "[PdfOp] Generating PDF..." << std::endl;
        PDFWriter writer;
        walk(vfs, in, Matrix::identity(), writer);
        auto pdf_bytes = writer.write();
        
        // 1. Primary Output: Shape Pass-through
        vfs->write<Shape>(fulfilling, in);

        // 2. Secondary Output: PDF bytes in 'file' port
        vfs->write<std::vector<uint8_t>>(fulfilling, pdf_bytes, "file");
        
        std::cout << "[PdfOp] Fulfilled Shape ($out) and PDF (file port, " << pdf_bytes.size() << " bytes)." << std::endl;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/pdf"},
            {"description", "Generates a PDF document from the spatial representation of the input shape."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"path", {{"type", "string"}, {"default", "export.pdf"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The input shape (pass-through)."}}},
                {"file", {{"type", "mime:application/pdf"}, {"description", "The generated PDF blob."}}}
            }}
        };
    }
};

static void pdf_init() {
    Processor::register_op<PdfOp<>, Shape, std::string>("jot/pdf");
}

} // namespace geo
} // namespace jotcad
