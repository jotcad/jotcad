#include "../include/ops.h"
#include <cmath>
#include <iostream>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

namespace jotcad {
namespace geo {

void makeHexagon(Geometry& geo, double radius, const std::string& variant) {
    double s3 = std::sqrt(3.0);
    if (variant == "middle") {
        geo.vertices.push_back({  radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ -radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ -radius/2.0, -radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({  radius/2.0, -radius * s3 / 2.0, 0.0 });
    } else if (variant == "cap") {
        geo.vertices.push_back({ radius, 0.0, 0.0 });
        geo.vertices.push_back({ radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ radius/2.0, -radius * s3 / 2.0, 0.0 });
    } else if (variant == "sector") {
        geo.vertices.push_back({ 0.0, 0.0, 0.0 });
        geo.vertices.push_back({ radius, 0.0, 0.0 });
        geo.vertices.push_back({ radius / 2.0, radius * s3 / 2.0, 0.0 });
    } else if (variant == "half") {
        geo.vertices.push_back({  radius, 0.0, 0.0 }); 
        geo.vertices.push_back({  radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
        geo.vertices.push_back({ -radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
        geo.vertices.push_back({ -radius, 0.0, 0.0 }); 
    } else {
        for (int i = 0; i < 6; ++i) {
            double angle_rad = (M_PI / 180.0) * (60.0 * i);
            geo.vertices.push_back({ radius * std::cos(angle_rad), radius * std::sin(angle_rad), 0.0 });
        }
    }

    Geometry::Face face;
    std::vector<int> loop;
    for (size_t i = 0; i < geo.vertices.size(); ++i) loop.push_back(i);
    face.loops.push_back(loop);
    geo.faces.push_back(face);
}

void applyOffset(Geometry& geo, double k) {
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
            tool.push_back(EK::Point_2(abs_k * cos(a), abs_k * sin(a)));
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

void applyOutline(Geometry& geo) {
    std::vector<std::array<int, 2>> segments;
    for (const auto& face : geo.faces) {
        for (const auto& loop : face.loops) {
            if (loop.empty()) continue;
            for (size_t i = 0; i < loop.size(); ++i) {
                segments.push_back({loop[i], loop[(i + 1) % loop.size()]});
            }
        }
    }
    geo.segments = segments;
    geo.faces.clear();
}

} // namespace geo
} // namespace jotcad
