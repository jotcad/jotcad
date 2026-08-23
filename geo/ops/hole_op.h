#pragma once
#include "protocols.h"
#include "processor.h"
#include "data/shape.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct HoleOp : P {
    static constexpr const char* path = "jot/hole";

    static Shape extract_holes(fs::VFSNode* vfs, const Shape& in) {
        Shape unwrap_in = in.unwrap_singleton();
        Shape out;
        out.tf = unwrap_in.tf;
        out.tags = unwrap_in.tags;

        if (unwrap_in.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>(unwrap_in.geometry.value());
            std::vector<Shape> hole_shapes;

            for (const auto& face : geo.faces) {
                if (face.loops.size() <= 1) continue; // No interior holes
                for (size_t li = 1; li < face.loops.size(); ++li) {
                    const auto& loop = face.loops[li]; // Interior hole loop
                    if (loop.size() < 2) continue;

                    Geometry loop_geo;
                    std::vector<int> new_loop;
                    for (int v_idx : loop) {
                        new_loop.push_back((int)loop_geo.vertices.size());
                        loop_geo.vertices.push_back(geo.vertices[v_idx]);
                    }
                    for (size_t i = 0; i < loop.size(); ++i) {
                        int next_i = (int)((i + 1) % loop.size());
                        loop_geo.segments.push_back({(int)i, next_i});
                    }
                    loop_geo.faces.push_back({ { new_loop } });
                    loop_geo.triangulate();

                    Shape hole_s;
                    hole_s.geometry = vfs->template materialize<Geometry>(loop_geo);
                    hole_s.add_tag("type", "face");
                    hole_shapes.push_back(hole_s);
                }
            }

            if (hole_shapes.size() == 1) {
                out.geometry = hole_shapes[0].geometry;
                out.tags = hole_shapes[0].tags;
            } else if (hole_shapes.size() > 1) {
                out.components = hole_shapes;
                out.add_tag("type", "group");
            }
        }

        for (const auto& child : unwrap_in.components) {
            Shape child_holes = extract_holes(vfs, child);
            if (child_holes.geometry.has_value() || !child_holes.components.empty()) {
                out.components.push_back(child_holes);
            }
        }

        return out.unwrap_singleton();
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = extract_holes(vfs, in);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/hole"},
            {"description", "Extracts the interior hole loop(s) of the input face(s) as closed 1D path(s)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The face or shape to extract interior holes from."}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The extracted interior hole loop shape(s)."}}}}}
        };
    }
};

static void hole_init(fs::VFSNode* vfs) {
    Processor::register_op<HoleOp<>, Shape>(vfs, "jot/hole");
    Processor::register_op<HoleOp<>, Shape>(vfs, "jot/holes");
}

} // namespace geo
} // namespace jotcad
