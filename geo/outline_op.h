#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/outline.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "op/outline";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, Shape& out) {
        // 1. Unwrap Geometry Selector
        auto geo_selector = in.geometry;
        
        // 2. Read Geometry Artifact (Raw bytes)
        auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
            geo_selector.path, 
            geo_selector.parameters, 
            {} 
        });

        // 3. Transform to World Space
        Geometry mesh;
        mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
        mesh.apply_tf(in.tf);

        // 4. Process
        applyOutline(mesh);

        // 5. Re-package (fresh mesh result)
        auto shape_data = P::write_shape(vfs, {}, mesh, {{"type", "outline"}});
        out = Shape::from_json(nlohmann::json::parse(shape_data));
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<Shape>(vfs, "$in", params, schema(), stack);
        Shape out;
        execute(vfs, in, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {{"$in", {{"type", "jot:shape"}}}}},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void outline_init() {
    Processor::register_op<OutlineOp<>>();
}

} // namespace geo
} // namespace jotcad
