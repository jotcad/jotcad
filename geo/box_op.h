#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct BoxOp : P {
    static constexpr const char* path = "jot/Box";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double width, double height, double depth) {
        Geometry res;
        double w = width;
        double h = height;
        double d = depth;

        // Bottom Vertices (z = -d/2)
        res.vertices.push_back({FT(-w/2), FT(-h/2), FT(-d/2)}); // 0
        res.vertices.push_back({FT(w/2), FT(-h/2), FT(-d/2)});  // 1
        res.vertices.push_back({FT(w/2), FT(h/2), FT(-d/2)});   // 2
        res.vertices.push_back({FT(-w/2), FT(h/2), FT(-d/2)});  // 3

        if (d == 0) {
            res.faces.push_back({{{0, 1, 2, 3}}});
        } else {
            // Top Vertices (z = d/2)
            res.vertices.push_back({FT(-w/2), FT(-h/2), FT(d/2)}); // 4
            res.vertices.push_back({FT(w/2), FT(-h/2), FT(d/2)});  // 5
            res.vertices.push_back({FT(w/2), FT(h/2), FT(d/2)});   // 6
            res.vertices.push_back({FT(-w/2), FT(h/2), FT(d/2)});  // 7

            // Corrected Winding (CCW from outside)
            res.faces.push_back({{{3, 2, 1, 0}}}); // Bottom (facing down)
            res.faces.push_back({{{4, 5, 6, 7}}}); // Top (facing up)
            res.faces.push_back({{{0, 1, 5, 4}}}); // Front
            res.faces.push_back({{{1, 2, 6, 5}}}); // Right
            res.faces.push_back({{{2, 3, 7, 6}}}); // Back
            res.faces.push_back({{{3, 0, 4, 7}}}); // Left
        }
        
        Shape out = P::make_shape(vfs, res, {{"type", "box"}});
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"width", "height", "depth"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Box"},
            {"description", "Generates a box (rectangle or cuboid)."},
            {"arguments", {
                {"width", {{"type", "number"}, {"default", 10.0}}},
                {"height", {{"type", "number"}, {"default", 10.0}}},
                {"depth", {{"type", "number"}, {"default", 0.0}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void box_init() {
    Processor::register_op<BoxOp<>, double, double, double>("jot/Box");
}

} // namespace geo
} // namespace jotcad
