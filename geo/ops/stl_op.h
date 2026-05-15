#pragma once
#include "protocols.h"
#include "processor.h"
#include "stl.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct StlOp : P {
    static constexpr const char* path = "jot/stl";

    static void walk(fs::VFSNode* vfs, const Shape& shape, STLWriter& writer) {
        Matrix current_tf = shape.tf;
        
        if (shape.geometry.has_value()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                geo.apply_tf(current_tf);
                writer.add_geometry(geo);
            } catch (const std::exception& e) {
                std::cerr << "[StlOp::walk] Error reading geometry: " << e.what() << std::endl;
            }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, writer);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& stl_path) {
        STLWriter writer;
        walk(vfs, in, writer);
        auto stl_bytes = writer.write_binary();
        
        // Output: STL bytes in the primary '$out' port
        vfs->write(fulfilling.with_output("$out"), stl_bytes);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/stl"},
            {"description", "Generates a binary STL file from the spatial representation of the input shape."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}},
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.stl"}}
            }},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "model/stl"}, {"description", "The generated STL blob."}}}
            }}
        };
    }
};

static void stl_init(fs::VFSNode* vfs) {
    Processor::register_op<StlOp<>, Shape, std::string>(vfs, "jot/stl");
}

} // namespace geo
} // namespace jotcad
