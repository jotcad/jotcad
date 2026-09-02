#pragma once
#include "kernel.h"
#include "predicates.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
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

    // Step 1: Repair topology so that kissing edges have 2 incoming faces each
    CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(mesh);

    std::map<Point_3, std::vector<Vertex_index>> coord_map;
    for (auto v : mesh.vertices()) coord_map[mesh.point(v)].push_back(v);

    // Map halfedges to canonical 3D spatial segments
    std::map<SpatialSegment, std::vector<Halfedge_index>> segment_map;
    for (auto h : mesh.halfedges()) {
        Point_3 p_src = mesh.point(mesh.source(h));
        Point_3 p_tgt = mesh.point(mesh.target(h));
        if (p_src == p_tgt) continue;
        SpatialSegment seg = (p_src < p_tgt) ? SpatialSegment(p_src, p_tgt) : SpatialSegment(p_tgt, p_src);
        segment_map[seg].push_back(h);
    }

    // Identify raw 1D kissing segments
    std::vector<SpatialSegment> raw_kissing_segments;
    std::set<Point_3> segment_endpoints;
    for (const auto& [seg, halfedges] : segment_map) {
        Point_3 pA = seg.first;
        Point_3 pB = seg.second;
        if (halfedges.size() >= 4 || (coord_map[pA].size() > 1 && coord_map[pB].size() > 1)) {
            raw_kissing_segments.push_back(seg);
            segment_endpoints.insert(pA);
            segment_endpoints.insert(pB);
        }
    }

    // Identify isolated 0D point touches (points with collisions not part of any kissing segment)
    std::vector<Point_3> isolated_contact_points;
    for (const auto& [pt, vs] : coord_map) {
        if (vs.size() > 1 && segment_endpoints.find(pt) == segment_endpoints.end()) {
            isolated_contact_points.push_back(pt);
        }
    }

    // Identify asymmetric Point-to-Edge contacts (0D vertex touching interior of 1D edge)
    for (const auto& [pt, vs] : coord_map) {
        for (const auto& [seg, halfedges] : segment_map) {
            if (is_point_on_segment<K>(pt, seg.first, seg.second)) {
                isolated_contact_points.push_back(pt);
                break;
            }
        }
    }

    // Identify asymmetric Point-to-Face contacts (0D vertex touching interior of 2D triangle)
    for (const auto& [pt, vs] : coord_map) {
        for (auto f : mesh.faces()) {
            auto h = mesh.halfedge(f);
            Point_3 A = mesh.point(mesh.source(h));
            Point_3 B = mesh.point(mesh.target(h));
            Point_3 C = mesh.point(mesh.target(mesh.next(h)));
            if (is_point_in_triangle<K>(pt, A, B, C)) {
                isolated_contact_points.push_back(pt);
                break;
            }
        }
    }

    if (raw_kissing_segments.empty() && isolated_contact_points.empty()) return false;

    // Step 2: Merge contiguous collinear kissing segments into maximal straight tools
    auto maximal_segments = merge_collinear_segments(raw_kissing_segments);

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

    // Step 3: Apply Minkowski Corefinement according to KissMode
    if (mode == KissMode::PART) {
        std::vector<Surface_mesh> shells;
        CGAL::Polygon_mesh_processing::split_connected_components(mesh, shells);
        if (shells.empty()) shells.push_back(std::move(mesh));

        for (auto& shell : shells) {
            for (auto tool : tools) {
                Surface_mesh result;
                bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
                    shell, tool, result,
                    CGAL::parameters::throw_on_self_intersection(false),
                    CGAL::parameters::throw_on_self_intersection(false),
                    CGAL::parameters::all_default()
                );
                if (ok && !result.is_empty() && CGAL::is_closed(result)) {
                    shell = std::move(result);
                }
            }
        }

        mesh.clear();
        for (const auto& shell : shells) {
            append_mesh(mesh, shell);
        }
    } else { // KissMode::WELD
        for (auto tool : tools) {
            Surface_mesh result;
            bool ok = CGAL::Polygon_mesh_processing::corefine_and_compute_union(
                mesh, tool, result,
                CGAL::parameters::throw_on_self_intersection(false),
                CGAL::parameters::throw_on_self_intersection(false),
                CGAL::parameters::all_default()
            );
            if (ok && !result.is_empty() && CGAL::is_closed(result)) {
                mesh = std::move(result);
            }
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
