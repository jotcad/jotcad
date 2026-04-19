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

    static void walk(jotcad::fs::VFSNode* vfs, const Shape& shape, const Matrix& parent_tf, PDFWriter& writer) {
        Matrix current_tf = parent_tf * Matrix::from_vec(shape.tf);
        
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->template read<Geometry>({
                    shape.geometry->path, 
                    shape.geometry->parameters
                });
                geo.apply_tf(current_tf.to_vec());
                writer.add_geometry(geo);
            } catch (...) { }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, current_tf, writer);
        }
    }

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& pdf_path, Shape& out) {
        PDFWriter writer;
        walk(vfs, in, Matrix::identity(), writer);
        auto pdf_bytes = writer.write();
        vfs->write(pdf_path, {}, pdf_bytes);
        out = in;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/pdf"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"path", {{"type", "jot:string"}, {"default", "export.pdf"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {
                {"$out", {{"type", "shape"}, {"alias", "$in"}}},
                {"file", {{"type", "file"}, {"mimeType", "application/pdf"}}}
            }}
        };
    }
    
    static std::vector<uint8_t> pdf_logic(jotcad::fs::VFSNode* vfs, const std::string& path, const nlohmann::json& params, const std::vector<std::string>& stack) {
        Shape in = Processor::decode<Shape>(vfs, "$in", params, schema(), stack);
        std::string pdf_path = params.value("path", "export.pdf");
        Shape out;
        execute(vfs, in, pdf_path, out);
        return P::write_shape_obj(out);
    }
};

static void pdf_init() {
    Processor::register_op("jot/pdf", PdfOp<>::pdf_logic, PdfOp<>::schema());
}

} // namespace geo
} // namespace jotcad
