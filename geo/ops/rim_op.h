#pragma once
#include "protocols.h"
#include "processor.h"
#include "data/shape.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RimOp : P {
    static constexpr const char* path = "jot/rim";

    static Shape extract_rim(fs::VFSNode* vfs, const Shape& in) {
        Shape unwrap_in = in.unwrap_singleton();
        Shape out;
        out.tf = unwrap_in.tf;
        out.tags = unwrap_in.tags;

        if (unwrap_in.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>(unwrap_in.geometry.value());
            std::vector<Shape> rim_shapes;

            for (const auto& face : geo.faces) {
                if (face.loops.empty()) continue;
                const auto& loop = face.loops[0]; // Outer boundary loop
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

                Shape rim_s;
                rim_s.geometry = vfs->template materialize<Geometry>(loop_geo);
                rim_s.add_tag("type", "face");
                rim_shapes.push_back(rim_s);
            }

            if (rim_shapes.size() == 1) {
                out.geometry = rim_shapes[0].geometry;
                out.tags = rim_shapes[0].tags;
            } else if (rim_shapes.size() > 1) {
                out.components = rim_shapes;
                out.add_tag("type", "group");
            }
        }

        for (const auto& child : unwrap_in.components) {
            Shape child_rim = extract_rim(vfs, child);
            if (child_rim.geometry.has_value() || !child_rim.components.empty()) {
                out.components.push_back(child_rim);
            }
        }

        return out.unwrap_singleton();
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = extract_rim(vfs, in);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rim"},
            {"description", "Extracts the outer boundary loop(s) of the input face(s) as closed 1D path(s)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The face or shape to extract the outer boundary rim from."}}}}},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The extracted outer boundary loop shape(s)."}}}}}
        };
    }
};

static void rim_init(fs::VFSNode* vfs) {
    Processor::register_op<RimOp<>, Shape>(vfs, "jot/rim");
    Processor::register_op<RimOp<>, Shape>(vfs, "jot/rims");
}

} // namespace geo
} // namespace jotcad
