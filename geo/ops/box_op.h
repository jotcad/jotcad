#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "fix/repair.h"
#include "../math/interval.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct BoxOp : P {
    static constexpr const char* path = "jot/Box";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval width, Interval height, Interval depth) {
        Geometry res;
        
        // Vertices at Interval min/max
        double x_min = width.min, x_max = width.max;
        double y_min = height.min, y_max = height.max;
        double z_min = depth.min, z_max = depth.max;

        bool dx = (x_min != x_max);
        bool dy = (y_min != y_max);
        bool dz = (z_min != z_max);
        int dims = (dx ? 1 : 0) + (dy ? 1 : 0) + (dz ? 1 : 0);

        std::string type = "closed";

        if (dims == 3) {
            // Full 3D Solid
            res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)}); // 0
            res.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)}); // 1
            res.vertices.push_back({FT(x_max), FT(y_max), FT(z_min)}); // 2
            res.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)}); // 3
            res.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)}); // 4
            res.vertices.push_back({FT(x_max), FT(y_min), FT(z_max)}); // 5
            res.vertices.push_back({FT(x_max), FT(y_max), FT(z_max)}); // 6
            res.vertices.push_back({FT(x_min), FT(y_max), FT(z_max)}); // 7

            res.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
            res.faces.push_back({{{4, 5, 6, 7}}}); // Top
            res.faces.push_back({{{0, 1, 5, 4}}}); // Front
            res.faces.push_back({{{1, 2, 6, 5}}}); // Right
            res.faces.push_back({{{2, 3, 7, 6}}}); // Back
            res.faces.push_back({{{3, 0, 4, 7}}}); // Left
            
            assert(fix::is_geometry_solid(boolean::Engine::geometry_to_mesh(res)));
            type = "closed";
        } else if (dims == 2) {
            // 2D Planar Surface
            if (!dz) { // XY Plane
                res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
                res.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)});
                res.vertices.push_back({FT(x_max), FT(y_max), FT(z_min)});
                res.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)});
            } else if (!dy) { // XZ Plane
                res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
                res.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)});
                res.vertices.push_back({FT(x_max), FT(y_min), FT(z_max)});
                res.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)});
            } else { // YZ Plane
                res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
                res.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)});
                res.vertices.push_back({FT(x_min), FT(y_max), FT(z_max)});
                res.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)});
            }
            res.faces.push_back({{{0, 1, 2, 3}}});
            type = "surface";
        } else if (dims == 1) {
            // 1D Segment
            res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
            res.vertices.push_back({FT(x_max), FT(y_max), FT(z_max)});
            res.segments.push_back({0, 1});
            type = "segments";
        } else {
            // 0D Point
            res.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
            res.points.push_back(0);
            type = "points";
        }
        
        Shape out = P::make_shape(vfs, res, {{"type", type}});
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"width", "height", "depth"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Box"},
            {"description", "Generates a box using symmetric or asymmetric intervals."},
            {"arguments", {
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "depth"}, {"type", "jot:interval"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void box_init(fs::VFSNode* vfs) {
    Processor::register_op<BoxOp<>, Interval, Interval, Interval>(vfs, "jot/Box");
}

} // namespace geo
} // namespace jotcad
