#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/points";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) { vfs->write<Shape>(fulfilling, in); return; }
        
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        for (size_t i = 0; i < geo.vertices.size(); ++i) {
            res.faces.push_back({{{(int)i}}}); // Point face
        }
        Shape out = in;
        out.geometry = vfs->write<Geometry>(res);
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/points"},
            {"description", "Extracts the vertices of the input shape as isolated points."},
            {"arguments", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to extract points from."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting points shape."}}}}}
        };
    }
};

static void points_init() {
    Processor::register_op<PointsOp<>, Shape>("jot/points");
}

} // namespace geo
} // namespace jotcad
