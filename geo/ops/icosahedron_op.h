#pragma once
#include "protocols.h"
#include "processor.h"
#include "fix/repair.h"
#include "math/interval.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct IcosahedronOp : P {
    static constexpr const char* path = "jot/Icosahedron";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, Interval width, Interval height, Interval depth, double edge) {
        Geometry res;

        Interval x_range = width;
        Interval y_range = (height.size() > 0) ? height : width;
        Interval z_range = (depth.size() > 0) ? depth : y_range;

        double w = x_range.size();
        double h = y_range.size();
        double d = z_range.size();
        double cx = x_range.center();
        double cy = y_range.center();
        double cz = z_range.center();

        // If explicit edge length is specified (edge > 0), calculate circumscribed diameter
        // For edge length s: Diameter D = s * sqrt(10 + 2*sqrt(5)) / 2 approx s * 1.902113032590307
        if (edge > 0.0) {
            double diam = edge * std::sqrt(10.0 + 2.0 * std::sqrt(5.0)) / 2.0;
            w = diam;
            h = diam;
            d = diam;
        }

        if (w <= 0.0) w = 10.0;
        if (h <= 0.0) h = w;
        if (d <= 0.0) d = h;

        FT w2 = FT(w) / FT(2);
        FT h2 = FT(h) / FT(2);
        FT d2 = FT(d) / FT(2);
        FT f_cx = FT(cx);
        FT f_cy = FT(cy);
        FT f_cz = FT(cz);

        // Unit icosahedron vertices normalized to circumscribed unit sphere (radius = 1.0)
        // Golden ratio phi = (1 + sqrt(5)) / 2
        // Length L = sqrt(1 + phi^2) = sqrt(10 + 2*sqrt(5)) / 2
        double phi = (1.0 + std::sqrt(5.0)) / 2.0;
        double L = std::sqrt(1.0 + phi * phi);
        double a = 1.0 / L;
        double b = phi / L;

        struct UnitVertex {
            double x, y, z;
        };

        std::vector<UnitVertex> unit_vertices = {
            {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
            { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
            { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a}
        };

        res.vertices.reserve(12);
        for (const auto& uv : unit_vertices) {
            res.vertices.push_back({
                f_cx + FT(uv.x) * w2,
                f_cy + FT(uv.y) * h2,
                f_cz + FT(uv.z) * d2
            });
        }

        // 20 Triangular Faces with outward normal (CCW) winding
        std::vector<std::vector<int>> faces = {
            {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
            {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
            {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
            {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
        };

        res.faces.reserve(20);
        for (const auto& f : faces) {
            res.faces.push_back({{{f[0], f[1], f[2]}}});
        }

        res.triangulate();

        // Assert closure and manifoldness
        assert(fix::is_geometry_solid(boolean::Engine::geometry_to_mesh(res)));

        Shape out = P::make_shape(vfs, res, {{"type", "closed"}});
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"width", "height", "depth", "edge"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Icosahedron"},
            {"dsl_name", "Icosahedron"},
            {"role", "constructor"},
            {"description", "Generates a 3D regular icosahedron solid."},
            {"synonyms", {"icosahedron", "Ico", "ico"}},
            {"inputs", nlohmann::json::object()},
            {"arguments", nlohmann::json::array({
                {{"name", "width"}, {"type", "jot:interval"}, {"default", 10.0}},
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "depth"}, {"type", "jot:interval"}, {"default", 0.0}},
                {{"name", "edge"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void icosahedron_init(fs::VFSNode* vfs) {
    Processor::register_op<IcosahedronOp<>, Interval, Interval, Interval, double>(vfs, "jot/Icosahedron");
}

} // namespace geo
} // namespace jotcad
