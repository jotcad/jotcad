#pragma once
#include "kernel.h"
#include "predicates.h"
#include "tool_builder.h"
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
    std::vector<std::vector<Point_3>> contact_convex_polygons;

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
            Point_3 p0 = (*tri)[0], p1 = (*tri)[1], p2 = (*tri)[2];
            if (p0 != p1 && p1 != p2 && p2 != p0) {
                contact_convex_polygons.push_back({p0, p1, p2});
            }
        } else if (const std::vector<Point_3>* poly = std::get_if<std::vector<Point_3>>(&*inter)) {
            if (poly->size() >= 3) {
                contact_convex_polygons.push_back(*poly);
            }
        }
    }

    if (raw_kissing_segments.empty() && contact_point_set.empty() && contact_convex_polygons.empty()) return false;

    // Step 2: Merge contiguous collinear kissing segments into maximal straight tools
    auto maximal_segments = merge_collinear_segments(raw_kissing_segments);
    std::vector<Point_3> isolated_contact_points(contact_point_set.begin(), contact_point_set.end());

    // Build true swept Minkowski tools via convex hull of endpoint delta-cubes
    std::vector<Surface_mesh> tools;
    for (const auto& seg : maximal_segments) {
        tools.push_back(make_minkowski_tool_from_feature(std::vector<Point_3>{seg.first, seg.second}, delta));
    }
    for (const auto& pt : isolated_contact_points) {
        tools.push_back(make_minkowski_tool_from_feature(std::vector<Point_3>{pt}, delta));
    }
    for (const auto& poly : contact_convex_polygons) {
        tools.push_back(make_minkowski_tool_from_feature(poly, delta));
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
