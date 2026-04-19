#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "jot/outline";

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
        
        // 2. Compute Outline (Extract unique segments from loops)
        Geometry outline;
        outline.vertices = geo.vertices;
        
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                if (loop.size() < 2) continue;
                for (size_t i = 0; i < loop.size(); ++i) {
                    int v1 = loop[i];
                    int v2 = loop[(i + 1) % loop.size()];
                    outline.segments.push_back({v1, v2});
                }
            }
        }

        // 3. Sink and Return
        std::string hash = vfs->write_with_cid("geo/mesh", outline);

        out = in;
        out.geometry = {"geo/mesh", {{"cid", hash}}};
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/outline"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void outline_init() {
    Processor::register_op<OutlineOp<>, Shape, Shape>("jot/outline");
}

} // namespace geo
} // namespace jotcad
