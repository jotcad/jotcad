#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/points";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, Shape& out) {
        if (!in.geometry.has_value()) {
            out = in;
            return;
        }

        // 1. Read input geometry
        Geometry geo = vfs->template read<Geometry>({
            in.geometry->path, 
            in.geometry->parameters
        });
        
        // 2. Clear faces/segments, keep only vertices
        Geometry points;
        points.vertices = geo.vertices;
        
        // 3. Add explicit point markers (indices to all vertices)
        for (size_t i = 0; i < points.vertices.size(); ++i) {
            points.points.push_back((int)i);
        }

        // 4. Sink and Return
        std::string hash = vfs->write_with_cid("geo/mesh", points);

        out = in;
        out.geometry = {"geo/mesh", {{"cid", hash}}};
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/points"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void points_init() {
    Processor::register_op<PointsOp<>, Shape, Shape>("jot/points");
}

} // namespace geo
} // namespace jotcad
