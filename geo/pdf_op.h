#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/pdf.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PdfOp : P {
    static constexpr const char* path = "jot/pdf";

    static void walk(jotcad::fs::VFSNode* vfs, const Shape& shape, std::vector<double> current_tf, PDFWriter& writer) {
        // Compose transformation: current_tf * shape.tf
        std::vector<double> next_tf(16, 0.0);
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 4; ++k) {
                    next_tf[i * 4 + j] += current_tf[i * 4 + k] * shape.tf[k * 4 + j];
                }
            }
        }

        if (!shape.geometry.path.empty()) {
            try {
                auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
                    shape.geometry.path, 
                    shape.geometry.parameters
                });
                
                Geometry geo; 
                geo.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
                geo.apply_tf(next_tf);
                
                writer.add_geometry(geo, shape.tags);
            } catch (const std::exception& e) {
                std::cerr << "[PdfOp] Warning: Could not resolve geometry " << shape.geometry.path << ": " << e.what() << std::endl;
            }
        }

        for (const auto& child : shape.components) {
            walk(vfs, child, next_tf, writer);
        }
    }

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& filename, double lineWidth, Shape& out) {
        PDFWriter::Config config;
        config.lineWidth = lineWidth;
        PDFWriter writer(config);
        
        std::vector<double> identity = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        
        walk(vfs, in, identity, writer);
        
        auto pdf_data = writer.write();
        
        // SINK: Export to VFS at the specific path requested. 
        // Correct signature: (path, parameters, data)
        vfs->write(filename, nlohmann::json::object(), pdf_data);

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
            {"outputs", {{"$out", {{"type", "shape"}}}}},
            {"metadata", {
                {"alias", "jot/pdf"},
                {"optimizeAliases", true} // Signal that this is a "Tee" operation
            }}
        };
    }
};

static void pdf_init() {
    Processor::register_op<PdfOp<>, Shape, std::string, double>();
}

} // namespace geo
} // namespace jotcad
