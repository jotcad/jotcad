#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include <cmath>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TriangleEquilateralOp : P {
    static constexpr const char* path = "jot/Triangle/equilateral";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& size) {
        Geometry res;
        double w = size.size() > 0 ? size[0] : 10.0;
        double h = (w * std::sqrt(3) / 2.0);

        res.vertices.push_back({FT(-w/2), FT(0), FT(0)});
        res.vertices.push_back({FT(w/2), FT(0), FT(0)});
        res.vertices.push_back({FT(0), FT(h), FT(0)});
        res.faces.push_back({{{0, 1, 2}}});

        Shape out = P::make_shape(vfs, res, {{"type", "triangle"}});
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"size"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle/equilateral"},
            {"description", "Generates an equilateral triangle."},
            {"arguments", {{"size", {{"type", "array"}, {"items", {{"type", "number"}}}, {"default", {10}}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void triangle_init() {
    Processor::register_op<TriangleEquilateralOp<>, std::vector<double>>("jot/Triangle/equilateral");
}

} // namespace geo
} // namespace jotcad
