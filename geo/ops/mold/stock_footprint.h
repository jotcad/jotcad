#pragma once
#include "types.h"
#include <CGAL/convex_hull_2.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <vector>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

struct StockFootprint {
    std::vector<CDT_Kernel::Point_2> outer_polygon;
    std::vector<std::vector<CDT_Kernel::Point_2>> hole_polygons;
    FT w_min = FT(0);
    FT w_max = FT(0);
};

/**
 * @brief Removes redundant collinear intermediate vertices along polygon edges.
 */
inline std::vector<CDT_Kernel::Point_2> simplify_collinear_polygon(const std::vector<CDT_Kernel::Point_2>& pts) {
    if (pts.size() <= 3) return pts;
    std::vector<CDT_Kernel::Point_2> cleaned;
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i) {
        const auto& prev = pts[(i + n - 1) % n];
        const auto& curr = pts[i];
        const auto& next = pts[(i + 1) % n];
        if (curr == prev || curr == next) continue;
        if (!CGAL::collinear(prev, curr, next)) {
            cleaned.push_back(curr);
        }
    }
    return cleaned;
}

/**
 * @brief Extracts the exact 2D projected boundary polygon (with any interior through-holes)
 *        and height bounds of a 3D stock mesh in the rotated draw frame (+Z).
 * 
 * Supports both convex shapes (OBBs / cuboids via convex hull) and arbitrary irregular,
 * non-convex, or hollow/annular stock polyhedra (via 2D Polygon_set_2 union of projected faces).
 */
inline StockFootprint compute_stock_footprint(
    const ExactMesh& stock_mesh,
    const CGAL::Aff_transformation_3<EK>& to_z
) {
    StockFootprint res;
    if (stock_mesh.is_empty() || stock_mesh.number_of_vertices() == 0) {
        return res;
    }

    std::vector<CDT_Kernel::Point_2> pts_2d;
    bool first = true;
    for (auto v : stock_mesh.vertices()) {
        auto p_rot = to_z(stock_mesh.point(v));
        FT u = p_rot.x();
        FT v_coord = p_rot.y();
        FT w = p_rot.z();
        if (first) {
            res.w_min = res.w_max = w;
            first = false;
        } else {
            if (w < res.w_min) res.w_min = w;
            if (w > res.w_max) res.w_max = w;
        }
        pts_2d.push_back(CDT_Kernel::Point_2(u, v_coord));
    }

    // For standard rectangular boxes and OBBs (<= 8 vertices or <= 12 faces),
    // 2D convex hull is exact, fast, and simple (no holes).
    if (stock_mesh.number_of_vertices() <= 8 || stock_mesh.number_of_faces() <= 12) {
        std::vector<CDT_Kernel::Point_2> hull;
        CGAL::convex_hull_2(pts_2d.begin(), pts_2d.end(), std::back_inserter(hull));
        res.outer_polygon = simplify_collinear_polygon(hull);
        return res;
    }

    // General irregular / non-convex / hollow stock mesh:
    // Union of all projected 2D triangular faces via CGAL::Polygon_set_2
    CGAL::Polygon_set_2<CDT_Kernel> pset;
    for (auto f : stock_mesh.faces()) {
        auto h = stock_mesh.halfedge(f);
        auto p0 = to_z(stock_mesh.point(stock_mesh.source(h)));
        auto p1 = to_z(stock_mesh.point(stock_mesh.target(h)));
        auto p2 = to_z(stock_mesh.point(stock_mesh.target(stock_mesh.next(h))));

        CDT_Kernel::Point_2 q0(p0.x(), p0.y());
        CDT_Kernel::Point_2 q1(p1.x(), p1.y());
        CDT_Kernel::Point_2 q2(p2.x(), p2.y());

        if (CGAL::collinear(q0, q1, q2)) continue;

        CGAL::Polygon_2<CDT_Kernel> tri;
        tri.push_back(q0);
        tri.push_back(q1);
        tri.push_back(q2);
        if (tri.is_clockwise_oriented()) tri.reverse_orientation();
        if (tri.is_simple()) {
            pset.join(tri);
        }
    }

    std::vector<CGAL::Polygon_with_holes_2<CDT_Kernel>> pwhs;
    pset.polygons_with_holes(std::back_inserter(pwhs));
    if (!pwhs.empty()) {
        size_t best_idx = 0;
        FT best_area = pwhs[0].outer_boundary().area();
        for (size_t i = 1; i < pwhs.size(); ++i) {
            FT a = pwhs[i].outer_boundary().area();
            if (a > best_area) {
                best_area = a;
                best_idx = i;
            }
        }
        const auto& pwh = pwhs[best_idx];
        const auto& ob = pwh.outer_boundary();
        std::vector<CDT_Kernel::Point_2> poly_pts;
        for (auto vit = ob.vertices_begin(); vit != ob.vertices_end(); ++vit) {
            poly_pts.push_back(*vit);
        }
        res.outer_polygon = simplify_collinear_polygon(poly_pts);

        // Extract any interior through-holes in the stock box
        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
            std::vector<CDT_Kernel::Point_2> hole_pts;
            for (auto vit = hit->vertices_begin(); vit != hit->vertices_end(); ++vit) {
                hole_pts.push_back(*vit);
            }
            if (!hole_pts.empty()) {
                res.hole_polygons.push_back(simplify_collinear_polygon(hole_pts));
            }
        }
    } else {
        std::vector<CDT_Kernel::Point_2> hull;
        CGAL::convex_hull_2(pts_2d.begin(), pts_2d.end(), std::back_inserter(hull));
        res.outer_polygon = simplify_collinear_polygon(hull);
    }

    return res;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
