#pragma once
#include "protocols.h"
#include "processor.h"
#include "algorithms/triangle.h"
#include <cmath>

namespace jotcad {
namespace geo {

// Exact rational approximations for triangle coordinates.
// sqrt(3)/2 approx 0.86602540378
static const FT TRI_SQRT3_2 = FT(86602540378LL) / FT(100000000000LL);

template <typename P = JotVfsProtocol>
struct TriangleEquilateralOp : P {
    static constexpr const char* path = "jot/Triangle/equilateral";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double size) {
        Geometry res;
        FT w = FT(size);
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
            {"arguments", {{{"name", "size"}, {"type", "jot:number"}, {"default", 10.0}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TriangleOp : P {
    static constexpr const char* path = "jot/Triangle";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double va, double vb, double vc) {
        Geometry res;
        makeTriangle(res, va, vb, vc);
        Shape out = P::make_shape(vfs, res, {{"type", "surface"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"va", "vb", "vc"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Triangle"},
            {"description", "Generates a 2D triangle centered on its centroid."},
            {"arguments", {
                {{"name", "va"}, {"type", "jot:number"}},
                {{"name", "vb"}, {"type", "jot:number"}},
                {{"name", "vc"}, {"type", "jot:number"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void triangle_init(fs::VFSNode* vfs) {
    Processor::register_op<TriangleOp<JotVfsProtocol>, double, double, double>(vfs, "jot/Triangle");
    Processor::register_op<TriangleEquilateralOp<JotVfsProtocol>, double>(vfs, "jot/Triangle/equilateral");
}

} // namespace geo
} // namespace jotcad
