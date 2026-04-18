#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/pdf.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PdfOp : P {
    static constexpr const char* path = "jot/pdf";

    static void walk(jotcad::fs::VFSNode* vfs, const Shape& shape, const Matrix& parent_tf, PDFWriter& writer) {
        Matrix current_tf = parent_tf * Matrix::from_vec(shape.tf);

        if (shape.geometry.has_value()) {
            try {
                auto geo = vfs->template read<Geometry>({
                    shape.geometry->path, 
                    shape.geometry->parameters,
                    {} // Empty stack
                });
                
                geo.apply_tf(current_tf.to_vec());
                writer.add_geometry(geo, shape.tags);
            } catch (const std::exception& e) {
                std::cerr << "[PdfOp] Warning: Could not resolve geometry " << shape.geometry->path << ": " << e.what() << std::endl;
            }
        }

        for (const auto& child : shape.components) {
            walk(vfs, child, current_tf, writer);
        }
    }

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& filename, double lineWidth, Shape& out) {
        // TEE: Pure functional pass-through. 
        out = in;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "filename", "lineWidth"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"filename", {{"type", "jot:string"}, {"default", "output.pdf"}}},
                {"lineWidth", {{"type", "jot:number"}, {"default", 0.096}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {
                {"$out", {{"type", "shape"}}},
                {"filename", {{"type", "file"}, {"mimeType", "application/pdf"}}}
            }},
            {"metadata", {
                {"alias", "jot/pdf"},
                {"passthrough", true}
            }}
        };
    }
};

static void pdf_init() {
    auto logic = [](jotcad::fs::VFSNode* vfs, const std::string& path, const json& params, const std::vector<std::string>& stack) -> std::vector<uint8_t> {
        if (params.contains("$port") && params["$port"] == "filename") {
            // PORT REQUEST: Generate and return the actual PDF bytes
            Shape in = Processor::decode<Shape>(vfs, "$in", params, PdfOp<>::schema(), stack);
            std::string filename = params.value("filename", "output.pdf");
            double lineWidth = params.value("lineWidth", 0.096);
            
            PDFWriter::Config config;
            config.lineWidth = lineWidth;
            PDFWriter writer(config);
            
            PdfOp<>::walk(vfs, in, Matrix::identity(), writer);
            return writer.write();
        }
        
        // DEFAULT: Pass-through geometry
        return Processor::logic_wrapper<PdfOp<>, Shape, Shape, std::string, double>(vfs, path, params, stack);
    };

    Processor::register_op("jot/pdf", logic, PdfOp<>::schema());
}

} // namespace geo
} // namespace jotcad
