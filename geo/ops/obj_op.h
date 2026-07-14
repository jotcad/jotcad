#pragma once
#include "protocols.h"
#include "processor.h"
#include "obj.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ObjOp : P {
    static constexpr const char* path = "jot/obj";

    static void walk(fs::VFSNode* vfs, const Shape& shape, OBJWriter& writer) {
        Matrix current_tf = shape.tf;
        
        if (shape.geometry.has_value() && !shape.is_gap()) {
            try {
                Geometry geo = vfs->read<Geometry>(shape.geometry.value());
                geo.apply_tf(current_tf);
                writer.add_geometry(geo);
            } catch (const std::exception& e) {
                std::cerr << "[ObjOp::walk] Error reading geometry: " << e.what() << std::endl;
            }
        }
        
        for (const auto& child : shape.components) {
            walk(vfs, child, writer);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& obj_path) {
        OBJWriter writer;
        walk(vfs, in, writer);
        std::string obj_text = writer.write_text();
        
        // Output: OBJ text bytes in the primary '$out' port
        std::vector<uint8_t> obj_bytes(obj_text.begin(), obj_text.end());
        vfs->write(fulfilling.with_output("$out"), obj_bytes);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "path"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/obj"},
            {"description", "Generates an OBJ file from the spatial representation of the input shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "path"}, {"type", "jot:string"}, {"default", "export.obj"}}
            })},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "model/obj"}, {"description", "The generated OBJ blob."}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ObjImportOp : P {
    static constexpr const char* path = "jot/Obj";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const fs::Selector& file) {
        fs::VFSResult file_res = vfs->read<fs::VFSResult>(file);
        
        std::string obj_text(file_res.data.begin(), file_res.data.end());
        
        Geometry geo;
        if (!OBJReader::read_text(obj_text, geo)) {
            throw std::runtime_error("Failed to parse OBJ file");
        }

        Shape out = P::make_shape(vfs, geo, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"file"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Obj"},
            {"description", "Imports an OBJ file from the VFS and returns its Shape representation."},
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

static void obj_init(fs::VFSNode* vfs) {
    Processor::register_op<ObjOp<>, Shape, std::string>(vfs, "jot/obj");
    Processor::register_op<ObjImportOp<>, fs::Selector>(vfs, "jot/Obj");
}

} // namespace geo
} // namespace jotcad
