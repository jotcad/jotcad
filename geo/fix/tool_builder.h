#pragma once
#include "kernel.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/convex_hull_3.h>
#include <vector>
#include <set>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace fix {

/**
 * make_box_mesh:
 * Constructs a watertight 2-manifold Surface_mesh for an axis-aligned bounding box.
 */
template <typename Point_3>
CGAL::Surface_mesh<Point_3> make_box_mesh(const Point_3& p_min, const Point_3& p_max) {
    typedef CGAL::Surface_mesh<Point_3> Mesh;
    Mesh box;
    auto x1 = p_min.x(), y1 = p_min.y(), z1 = p_min.z();
    auto x2 = p_max.x(), y2 = p_max.y(), z2 = p_max.z();

    auto v0 = box.add_vertex(Point_3(x1, y1, z1));
    auto v1 = box.add_vertex(Point_3(x2, y1, z1));
    auto v2 = box.add_vertex(Point_3(x2, y2, z1));
    auto v3 = box.add_vertex(Point_3(x1, y2, z1));
    auto v4 = box.add_vertex(Point_3(x1, y1, z2));
    auto v5 = box.add_vertex(Point_3(x2, y1, z2));
    auto v6 = box.add_vertex(Point_3(x2, y2, z2));
    auto v7 = box.add_vertex(Point_3(x1, y2, z2));

    box.add_face(v0, v3, v2); box.add_face(v0, v2, v1); // Bottom
    box.add_face(v4, v5, v6); box.add_face(v4, v6, v7); // Top
    box.add_face(v0, v1, v5); box.add_face(v0, v5, v4); // Front
    box.add_face(v2, v3, v7); box.add_face(v2, v7, v6); // Back
    box.add_face(v0, v4, v7); box.add_face(v0, v7, v3); // Left
    box.add_face(v1, v2, v6); box.add_face(v1, v6, v5); // Right
    return box;
}

/**
 * make_minkowski_tool_from_feature:
 * Constructs the exact Minkowski sum S \oplus B_\infty(\delta) by placing axis-aligned
 * \delta-cubes at each feature vertex and taking their 3D convex hull.
 * Natively supports 0D Points, 1D Segments, and 2D Convex Polygons.
 */
template <typename Point_3, typename FT = typename CGAL::Kernel_traits<Point_3>::Kernel::FT>
CGAL::Surface_mesh<Point_3> make_minkowski_tool_from_feature(
    const std::vector<Point_3>& feature_points,
    const FT& delta
) {
    std::vector<Point_3> hull_points;
    for (const auto& pt : feature_points) {
        hull_points.push_back(Point_3(pt.x() - delta, pt.y() - delta, pt.z() - delta));
        hull_points.push_back(Point_3(pt.x() + delta, pt.y() - delta, pt.z() - delta));
        hull_points.push_back(Point_3(pt.x() + delta, pt.y() + delta, pt.z() - delta));
        hull_points.push_back(Point_3(pt.x() - delta, pt.y() + delta, pt.z() - delta));
        hull_points.push_back(Point_3(pt.x() - delta, pt.y() - delta, pt.z() + delta));
        hull_points.push_back(Point_3(pt.x() + delta, pt.y() - delta, pt.z() + delta));
        hull_points.push_back(Point_3(pt.x() + delta, pt.y() + delta, pt.z() + delta));
        hull_points.push_back(Point_3(pt.x() - delta, pt.y() + delta, pt.z() + delta));
    }
    CGAL::Surface_mesh<Point_3> tool;
    CGAL::convex_hull_3(hull_points.begin(), hull_points.end(), tool);
    return tool;
}

/**
 * append_mesh:
 * Appends all vertices and faces of src mesh into tgt mesh.
 */
template <typename Point_3>
void append_mesh(CGAL::Surface_mesh<Point_3>& tgt, const CGAL::Surface_mesh<Point_3>& src) {
    typedef CGAL::Surface_mesh<Point_3> Mesh;
    std::map<typename Mesh::Vertex_index, typename Mesh::Vertex_index> v_map;
    for (auto v : src.vertices()) {
        v_map[v] = tgt.add_vertex(src.point(v));
    }
    for (auto f : src.faces()) {
        std::vector<typename Mesh::Vertex_index> face_verts;
        for (auto h : src.halfedges_around_face(src.halfedge(f))) {
            face_verts.push_back(v_map[src.target(h)]);
        }
        tgt.add_face(face_verts);
    }
}

/**
 * merge_collinear_segments:
 * Chained collinear segment merger: merges adjacent collinear segments into maximal straight lines.
 */
template <typename Point_3, typename FT = typename CGAL::Kernel_traits<Point_3>::Kernel::FT>
std::vector<std::pair<Point_3, Point_3>> merge_collinear_segments(
    const std::vector<std::pair<Point_3, Point_3>>& raw_segments,
    const FT& max_sin_sq = FT(1) / FT(10000)
) {
    typedef std::pair<Point_3, Point_3> SpatialSegment;
    if (raw_segments.empty()) return {};

    std::vector<bool> used(raw_segments.size(), false);
    std::vector<SpatialSegment> merged;

    for (size_t i = 0; i < raw_segments.size(); ++i) {
        if (used[i]) continue;
        used[i] = true;
        Point_3 p_start = raw_segments[i].first;
        Point_3 p_end = raw_segments[i].second;

        bool extended = true;
        while (extended) {
            extended = false;
            auto dir = p_end - p_start;
            auto dir_len_sq = dir.squared_length();
            if (dir_len_sq == 0) break;

            for (size_t j = 0; j < raw_segments.size(); ++j) {
                if (used[j]) continue;
                Point_3 q1 = raw_segments[j].first;
                Point_3 q2 = raw_segments[j].second;

                auto try_extend = [&](const Point_3& match_pt, const Point_3& ext_pt, bool is_end) -> bool {
                    auto next_dir = is_end ? (ext_pt - match_pt) : (match_pt - ext_pt);
                    auto next_len_sq = next_dir.squared_length();
                    if (next_len_sq == 0) return false;
                    if (dir * next_dir <= 0) return false; // Opposing direction
                    auto cross = CGAL::cross_product(dir, next_dir);
                    if (cross.squared_length() <= max_sin_sq * dir_len_sq * next_len_sq) {
                        if (is_end) p_end = ext_pt;
                        else p_start = ext_pt;
                        used[j] = true;
                        extended = true;
                        return true;
                    }
                    return false;
                };

                if (q1 == p_end && try_extend(q1, q2, true)) break;
                if (q2 == p_end && try_extend(q2, q1, true)) break;
                if (q2 == p_start && try_extend(q2, q1, false)) break;
                if (q1 == p_start && try_extend(q1, q2, false)) break;
            }
        }
        merged.push_back({p_start, p_end});
    }
    return merged;
}

} // namespace fix
} // namespace geo
} // namespace jotcad
