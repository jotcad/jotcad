#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/geometry.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PathOp : P {
    static constexpr const char* path = "op/path";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, bool closed, Shape& out) {
        auto geo_selector = in.geometry;
        auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
            geo_selector.path, 
            geo_selector.parameters, 
            {} 
        });

        Geometry mesh;
        mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
        mesh.apply_tf(in.tf);

        Geometry out_geo;
        out_geo.vertices = mesh.vertices;
        if (mesh.vertices.size() >= 2) {
            for (size_t i = 0; i < mesh.vertices.size() - 1; ++i) out_geo.segments.push_back({(int)i, (int)(i + 1)});
            if (closed && mesh.vertices.size() > 2) out_geo.segments.push_back({(int)mesh.vertices.size() - 1, 0});
        }

        auto shape_data = P::write_shape(vfs, {}, out_geo, {{"type", closed ? "loop" : "link"}});
        out = Shape::from_json(nlohmann::json::parse(shape_data));
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<Shape>(vfs, "$in", params, schema(), stack);
        bool closed = (path == "jot/loop");
        Shape out;
        execute(vfs, in, closed, out);
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

static void path_init() {
    Processor::Operation link;
    link.path = "jot/link";
    link.logic = PathOp<>::logic;
    link.schema = PathOp<>::schema();
    Processor::register_op(link);

    Processor::Operation loop = link;
    loop.path = "jot/loop";
    loop.logic = PathOp<>::logic;
    loop.schema = PathOp<>::schema();
    Processor::register_op(loop);
}

} // namespace geo
} // namespace jotcad
