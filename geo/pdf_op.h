#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include <fstream>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PdfOp : P {
    static constexpr const char* path = "op/pdf";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& filename, Shape& out) {
        // 1. Unwrap & Transform (Standard Contract)
        auto geo_selector = in.geometry;
        auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
            geo_selector.path, 
            geo_selector.parameters, 
            {} 
        });

        Geometry mesh;
        mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
        mesh.apply_tf(in.tf);

        // 2. Side Effect (Write PDF)
        std::ofstream os(filename);
        os << "%PDF-1.4 Mock\n";
        os << "%% JOT Geometry Export\n";
        os << "Vertices: " << mesh.vertices.size() << "\n";
        os << "Faces: " << mesh.faces.size() << "\n";
        os.close();

        // 3. Tee Pattern: Pass input shape through to $out
        out = in;
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<Shape>(vfs, "$in", params, schema(), stack);
        auto filename = Processor::decode<std::string>(vfs, "path", params, schema(), stack);
        
        Shape out;
        execute(vfs, in, filename, out);
        
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"path", {{"type", "jot:string"}, {"default", "export.pdf"}}}
            }},
            {"inputs", {
                {"$in", {{"type", "shape"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void pdf_init() {
    Processor::register_op<PdfOp<>>();
}

} // namespace geo
} // namespace jotcad
