#pragma once
#include "geometry.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>
#include <vector>
#include <cmath>

namespace jotcad {
namespace geo {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

static void applyOffset(Geometry& geo, double k) {
    if (k == 0) return;
    for (auto& face : geo.faces) {
        if (face.loops.empty()) continue;
        Polygon_2 boundary;
        for (int idx : face.loops[0]) {
            boundary.push_back(EK::Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
        }
        if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();

        Polygon_2 tool;
        int segments = 16;
        double abs_k = std::abs(k);
        for (int i = 0; i < segments; ++i) {
            double a = 2.0 * M_PI * i / segments;
            tool.push_back(EK::Point_2(abs_k * std::cos(a), abs_k * std::sin(a)));
        }

        if (k > 0) {
            Polygon_with_holes_2 offset_pwh = CGAL::minkowski_sum_2(boundary, tool);
            face.loops.clear();
            std::vector<int> new_loop;
            const auto& ob = offset_pwh.outer_boundary();
            for (auto it = ob.vertices_begin(); it != ob.vertices_end(); ++it) {
                new_loop.push_back(geo.vertices.size());
                geo.vertices.push_back({CGAL::to_double(it->x()), CGAL::to_double(it->y()), 0.0});
            }
            face.loops.push_back(new_loop);
        }
    }
}

} // namespace geo
} // namespace jotcad
