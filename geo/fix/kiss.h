#pragma once
#include "kernel.h"
#include "predicates.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/intersections.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace fix {

enum class KissMode {
    PART, // Carve clearance gap via Minkowski Difference
    WELD  // Add structural bridge via Minkowski Union
};

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

    box.add_face(v0, v2, v1); box.add_face(v0, v3, v2);
    box.add_face(v4, v5, v6); box.add_face(v4, v6, v7);
    box.add_face(v0, v1, v5); box.add_face(v0, v5, v4);
    box.add_face(v2, v3, v7); box.add_face(v2, v7, v6);
    box.add_face(v3, v0, v4); box.add_face(v3, v4, v7);
    box.add_face(v1, v2, v6); box.add_face(v1, v6, v5);

    return box;
}

template <typename Mesh>
void append_mesh(Mesh& target, const Mesh& source) {
    typedef typename Mesh::Vertex_index Vertex_index;
    std::map<Vertex_index, Vertex_index> v_map;
    for (auto v : source.vertices()) {
        v_map[v] = target.add_vertex(source.point(v));
    }
    for (auto f : source.faces()) {
        std::vector<Vertex_index> f_verts;
        for (auto fv : source.vertices_around_face(source.halfedge(f))) {
            f_verts.push_back(v_map[fv]);
        }
        target.add_face(f_verts);
    }
}

template <typename Point_3>
std::vector<std::pair<Point_3, Point_3>> merge_collinear_segments(
    const std::vector<std::pair<Point_3, Point_3>>& raw_segments
) {
    if (raw_segments.empty()) return {};

    std::vector<std::pair<Point_3, Point_3>> merged;
    std::vector<bool> used(raw_segments.size(), false);

    for (size_t i = 0; i < raw_segments.size(); ++i) {
        if (used[i]) continue;
        Point_3 p_start = raw_segments[i].first;
        Point_3 p_end = raw_segments[i].second;
        used[i] = true;

        bool extended = true;
        while (extended) {
            extended = false;
            auto dir = p_end - p_start;
            for (size_t j = 0; j < raw_segments.size(); ++j) {
                if (used[j]) continue;
                Point_3 q1 = raw_segments[j].first;
                Point_3 q2 = raw_segments[j].second;

                // Check if adjacent to p_end and collinear
                if (q1 == p_end) {
                    auto next_dir = q2 - q1;
                    if (CGAL::cross_product(dir, next_dir) == CGAL::NULL_VECTOR && dir * next_dir > 0) {
                        p_end = q2;
                        used[j] = true;
                        extended = true;
                        break;
                    }
                } else if (q2 == p_end) {
                    auto next_dir = q1 - q2;
                    if (CGAL::cross_product(dir, next_dir) == CGAL::NULL_VECTOR && dir * next_dir > 0) {
                        p_end = q1;
                        used[j] = true;
                        extended = true;
                        break;
                    }
                }
                // Check if adjacent to p_start and collinear
                else if (q2 == p_start) {
                    auto prev_dir = q1 - q2;
                    if (CGAL::cross_product(dir, prev_dir) == CGAL::NULL_VECTOR && dir * prev_dir > 0) {
                        p_start = q1;
                        used[j] = true;
                        extended = true;
                        break;
                    }
                } else if (q1 == p_start) {
                    auto prev_dir = q2 - q1;
                    if (CGAL::cross_product(dir, prev_dir) == CGAL::NULL_VECTOR && dir * prev_dir > 0) {
                        p_start = q2;
                        used[j] = true;
                        extended = true;
                        break;
                    }
                }
            }
        }
        merged.push_back({p_start, p_end});
    }
    return merged;
}

template <typename Mesh, typename K = EK>
typename K::Triangle_3 get_face_triangle(const Mesh& mesh, typename Mesh::Face_index f) {
    auto h = mesh.halfedge(f);
    return typename K::Triangle_3(
        mesh.point(mesh.source(h)),
        mesh.point(mesh.target(h)),
        mesh.point(mesh.target(mesh.next(h)))
    );
}

/**
 * resolve_kissing_seams:
 * Resolves zero-volume contact singularities (kissing edges, kissing curves, and point touches)
 * via dual strategies:
 *   - KissMode::PART: Minkowski Difference (carves physical clearance gap)
 *   - KissMode::WELD: Minkowski Union (adds physical structural bridge)
 */
template <typename K = EK>
bool resolve_kissing_seams(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, 
    KissMode mode = KissMode::PART,
    typename K::FT delta = 0.01
) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename Surface_mesh::Halfedge_index Halfedge_index;
    typedef typename K::Point_3 Point_3;
    typedef typename K::FT FT;
    typedef std::pair<Point_3, Point_3> SpatialSegment;

    // Step 1: Repair topology so that non-manifold kissing edges have 2 incoming faces each
    CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(mesh);

    // Extract all colliding non-adjacent face pairs via exact AABB spatial search
    std::vector<std::pair<typename Surface_mesh::Face_index, typename Surface_mesh::Face_index>> colliding_pairs;
    CGAL::Polygon_mesh_processing::self_intersections(mesh, std::back_inserter(colliding_pairs));

    std::set<Point_3> contact_point_set;
    std::vector<SpatialSegment> raw_kissing_segments;

    for (const auto& [f1, f2] : colliding_pairs) {
        auto t1 = get_face_triangle<Surface_mesh, K>(mesh, f1);
        auto t2 = get_face_triangle<Surface_mesh, K>(mesh, f2);
        auto inter = CGAL::intersection(t1, t2);
        if (!inter) continue;

        if (const Point_3* pt = std::get_if<Point_3>(&*inter)) {
            contact_point_set.insert(*pt);
        } else if (const typename K::Segment_3* seg = std::get_if<typename K::Segment_3>(&*inter)) {
            Point_3 p1 = seg->source();
            Point_3 p2 = seg->target();
            if (p1 != p2) {
                SpatialSegment s = (p1 < p2) ? SpatialSegment(p1, p2) : SpatialSegment(p2, p1);
                raw_kissing_segments.push_back(s);
            }
        } else if (const typename K::Triangle_3* tri = std::get_if<typename K::Triangle_3>(&*inter)) {
            for (int i = 0; i < 3; ++i) {
                Point_3 p1 = (*tri)[i];
                Point_3 p2 = (*tri)[(i + 1) % 3];
                if (p1 != p2) {
                    SpatialSegment s = (p1 < p2) ? SpatialSegment(p1, p2) : SpatialSegment(p2, p1);
                    raw_kissing_segments.push_back(s);
                }
            }
        } else if (const std::vector<Point_3>* poly = std::get_if<std::vector<Point_3>>(&*inter)) {
            size_t n = poly->size();
            for (size_t i = 0; i < n; ++i) {
                Point_3 p1 = (*poly)[i];
                Point_3 p2 = (*poly)[(i + 1) % n];
                if (p1 != p2) {
                    SpatialSegment s = (p1 < p2) ? SpatialSegment(p1, p2) : SpatialSegment(p2, p1);
                    raw_kissing_segments.push_back(s);
                }
            }
        }
    }

    if (raw_kissing_segments.empty() && contact_point_set.empty()) return false;

    // Step 2: Merge contiguous collinear kissing segments into maximal straight tools
    auto maximal_segments = merge_collinear_segments(raw_kissing_segments);
    std::vector<Point_3> isolated_contact_points(contact_point_set.begin(), contact_point_set.end());

    // Build Minkowski boxes (cutters for PART, weld bridges for WELD)
    std::vector<Surface_mesh> tools;
    for (const auto& seg : maximal_segments) {
        Point_3 p1 = seg.first;
        Point_3 p2 = seg.second;
        Point_3 p_min(
            (std::min)(p1.x(), p2.x()) - delta,
            (std::min)(p1.y(), p2.y()) - delta,
            (std::min)(p1.z(), p2.z()) - delta
        );
        Point_3 p_max(
            (std::max)(p1.x(), p2.x()) + delta,
            (std::max)(p1.y(), p2.y()) + delta,
            (std::max)(p1.z(), p2.z()) + delta
        );
        tools.push_back(make_box_mesh(p_min, p_max));
    }
    for (const auto& pt : isolated_contact_points) {
        Point_3 p_min(pt.x() - delta, pt.y() - delta, pt.z() - delta);
        Point_3 p_max(pt.x() + delta, pt.y() + delta, pt.z() + delta);
        tools.push_back(make_box_mesh(p_min, p_max));
    }

    // Step 3: Combine tools into a unified Minkowski tool to ensure single-pass Corefinement
    Surface_mesh unified_tool;
    if (!tools.empty()) {
        unified_tool = tools[0];
        for (size_t i = 1; i < tools.size(); ++i) {
            Surface_mesh u_res;
            if (CGAL::Polygon_mesh_processing::corefine_and_compute_union(
                    unified_tool, tools[i], u_res,
                    CGAL::parameters::throw_on_self_intersection(false),
                    CGAL::parameters::throw_on_self_intersection(false),
                    CGAL::parameters::all_default()) && !u_res.is_empty() && CGAL::is_closed(u_res)) {
                unified_tool = std::move(u_res);
            } else {
                append_mesh(unified_tool, tools[i]);
            }
        }
    }

    // Step 4: Apply Minkowski Corefinement according to KissMode
    if (mode == KissMode::PART) {
        std::vector<Surface_mesh> shells;
        CGAL::Polygon_mesh_processing::split_connected_components(mesh, shells);
        if (shells.empty()) shells.push_back(std::move(mesh));

        for (auto& shell : shells) {
            Surface_mesh result;
            bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
                shell, unified_tool, result,
                CGAL::parameters::throw_on_self_intersection(false),
                CGAL::parameters::throw_on_self_intersection(false),
                CGAL::parameters::all_default()
            );
            if (ok && !result.is_empty() && CGAL::is_closed(result)) {
                shell = std::move(result);
            }
        }

        mesh.clear();
        for (const auto& shell : shells) {
            append_mesh(mesh, shell);
        }
    } else { // KissMode::WELD
        Surface_mesh result;
        bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
            mesh, unified_tool, result,
            CGAL::parameters::throw_on_self_intersection(false),
            CGAL::parameters::throw_on_self_intersection(false),
            CGAL::parameters::all_default()
        );
        if (ok && !result.is_empty() && CGAL::is_closed(result)) {
            mesh = std::move(result);
        }
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    mesh.collect_garbage();
    return true;
}

template <typename K = EK>
inline bool separate_kissing_columns(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, 
    typename K::FT delta = 0.01,
    typename K::FT collar_len = 0.01
) {
    return resolve_kissing_seams<K>(mesh, KissMode::PART, delta);
}

template <typename K = EK>
inline bool weld_kissing_columns(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, 
    typename K::FT delta = 0.01
) {
    return resolve_kissing_seams<K>(mesh, KissMode::WELD, delta);
}

} // namespace fix
} // namespace geo
} // namespace jotcad
