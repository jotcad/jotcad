#pragma once
#include "protocols.h"
#include "processor.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximations for triangle coordinates.
// sqrt(3)/2 approx 0.86602540378
static const FT TRI_SQRT3_2 = FT(86602540378LL) / FT(100000000000LL);

template <typename P = JotVfsProtocol>
struct TriangleEquilateralOp : P {
    static constexpr const char* path = "jot/Triangle/equilateral";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& size) {
        Geometry res;
        FT w = size.size() > 0 ? FT(size[0]) : FT(10);
        FT h = w * TRI_SQRT3_2;

        res.vertices.push_back({-w/FT(2), FT(0), FT(0)});
        res.vertices.push_back({w/FT(2), FT(0), FT(0)});
        res.vertices.push_back({FT(0), h, FT(0)});
        res.faces.push_back({{{0, 1, 2}}});

        Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"size"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle/equilateral"},
            {"description", "Generates an equilateral triangle."},
            {"arguments", {{{"name", "size"}, {"type", "jot:numbers"}, {"default", {10}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void triangle_init(fs::VFSNode* vfs) {
    Processor::register_op<TriangleEquilateralOp<>, std::vector<double>>(vfs, "jot/Triangle/equilateral");
}

} // namespace geo
} // namespace jotcad
