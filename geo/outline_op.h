#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "jot/outline";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) { vfs->write<Shape>(fulfilling, in); return; }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                for (size_t i = 0; i < loop.size(); ++i) {
                    int v1 = loop[i];
                    int v2 = loop[(i + 1) % loop.size()];
                    int base = (int)res.vertices.size();
                    res.vertices.push_back(geo.vertices[v1]);
                    res.vertices.push_back(geo.vertices[v2]);
                    res.segments.push_back({base, base + 1});
                }
            }
        }
        Shape out = in;
        out.geometry = vfs->write<Geometry>(res);
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/outline"},
            {"description", "Extracts the boundary edges of the input shape as line segments."},
            {"arguments", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to outline."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting wireframe/outline shape."}}}}}
        };
    }
};

static void outline_init() {
    Processor::register_op<OutlineOp<>, Shape>("jot/outline");
}

} // namespace geo
} // namespace jotcad
