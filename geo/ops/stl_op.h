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
        
        if (shape.geometry.has_value() && !shape.is_gap()) {
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
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.stl"}}
            })},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "model/stl"}, {"description", "The generated STL blob."}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct StlImportOp : P {
    static constexpr const char* path = "jot/Stl";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const fs::Selector& file) {
        fs::VFSResult file_res = vfs->read<fs::VFSResult>(file);
        
        Geometry geo;
        bool success = false;
        if (file_res.data.size() >= 84) {
            success = STLReader::read_binary(file_res.data, geo);
        }
        if (!success) {
            std::string stl_text(file_res.data.begin(), file_res.data.end());
            success = STLReader::read_ascii(stl_text, geo);
        }

        if (!success) {
            throw std::runtime_error("Failed to parse STL file");
        }

        Shape out = P::make_shape(vfs, geo, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"file"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Stl"},
            {"description", "Imports an STL file from the VFS and returns its Shape representation."},
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "file"}, {"type", "jot:file"}}
            })},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The imported shape."}}}
            }}
        };
    }
};

static void stl_init(fs::VFSNode* vfs) {
    Processor::register_op<StlOp<>, Shape, std::string>(vfs, "jot/stl");
    Processor::register_op<StlImportOp<>, fs::Selector>(vfs, "jot/Stl");
}

} // namespace geo
} // namespace jotcad
