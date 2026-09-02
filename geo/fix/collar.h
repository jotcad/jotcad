#pragma once
#include "kernel.h"
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <cmath>

namespace jotcad {
namespace geo {
namespace fix {

// Max fraction of edge length allocated to transition collar (midpoint clamp)
inline constexpr int kMaxCollarEdgeSubdivisionDenominator = 2;

/**
 * insert_transition_collars:
 * Subdivides edges rising from single-vertex base anchors into kissing arms,
 * placing collar vertices at safe bounded distance: min(collar_len, L / 2).
 */
template <typename Mesh, typename CoordMap, typename FT>
std::vector<typename Mesh::Vertex_index> insert_transition_collars(
    Mesh& mesh, 
    const CoordMap& coord_map, 
    const FT& collar_len
) {
    typedef typename Mesh::Halfedge_index Halfedge_index;
    typedef typename Mesh::Vertex_index Vertex_index;
    typedef typename Mesh::Point Point_3;
    typedef std::pair<Point_3, Point_3> SpatialSegment;

    // Step 1: Map halfedges to canonical 3D spatial segments
    std::map<SpatialSegment, std::vector<Halfedge_index>> segment_map;
    for (auto h : mesh.halfedges()) {
        Point_3 p_src = mesh.point(mesh.source(h));
        Point_3 p_tgt = mesh.point(mesh.target(h));
        if (p_src == p_tgt) continue;
        SpatialSegment seg = (p_src < p_tgt) ? SpatialSegment(p_src, p_tgt) : SpatialSegment(p_tgt, p_src);
        segment_map[seg].push_back(h);
    }

    // Step 2: Split only kissing edges (>= 4 halfedges) rising from single-vertex anchors
    std::vector<std::pair<Halfedge_index, FT>> edges_to_split;
    for (const auto& [seg, halfedges] : segment_map) {
        if (halfedges.size() >= 4) {
            for (Halfedge_index h : halfedges) {
                Vertex_index u = mesh.source(h);
                Vertex_index v = mesh.target(h);
                if (coord_map.at(mesh.point(u)).size() == 1 && coord_map.at(mesh.point(v)).size() > 1) {
                    Point_3 pu = mesh.point(u);
                    Point_3 pv = mesh.point(v);
                    FT sq_len = (pv - pu).squared_length();
                    if (sq_len > FT(0)) {
                        FT c_sq = collar_len * collar_len;
                        FT t = (sq_len <= FT(4) * c_sq) 
                            ? (FT(1) / FT(kMaxCollarEdgeSubdivisionDenominator)) 
                            : (collar_len / CGAL::approximate_sqrt(sq_len));
                        edges_to_split.push_back({h, t});
                    }
                }
            }
        }
    }

    std::vector<Vertex_index> new_collar_verts;
    for (const auto& item : edges_to_split) {
        Halfedge_index eh = item.first;
        FT t = item.second;
        Point_3 pu = mesh.point(mesh.source(eh));
        Point_3 pv = mesh.point(mesh.target(eh));
        Point_3 split_pt = pu + (pv - pu) * t;
        
        Halfedge_index new_h = CGAL::Euler::split_edge(eh, mesh);
        Vertex_index new_v = mesh.target(new_h);
        mesh.point(new_v) = split_pt;
        new_collar_verts.push_back(new_v);
    }
    return new_collar_verts;
}

/**
 * retract_contact_vertices:
 * Computes intrinsic umbrella normal sums and displaces vertices inward into their solid volume.
 */
template <typename Mesh, typename VertexList, typename FT>
void retract_contact_vertices(Mesh& mesh, const VertexList& vertices, const FT& delta_max) {
    typedef typename Mesh::Point Point_3;
    typedef typename CGAL::Kernel_traits<Point_3>::Kernel::Vector_3 Vector_3;

    for (auto v : vertices) {
        Vector_3 normal_sum(0, 0, 0);
        auto h = mesh.halfedge(v);
        if (h == Mesh::null_halfedge()) continue;

        for (auto f : mesh.faces_around_target(h)) {
            if (f == Mesh::null_face()) continue;
            std::vector<Point_3> pts;
            for (auto fv : mesh.vertices_around_face(mesh.halfedge(f))) {
                pts.push_back(mesh.point(fv));
            }
            if (pts.size() == 3) {
                normal_sum += CGAL::cross_product(pts[1] - pts[0], pts[2] - pts[0]);
            }
        }

        FT sq_len = normal_sum.squared_length();
        if (sq_len <= FT(0)) continue;

        FT approx_len = CGAL::approximate_sqrt(sq_len);
        if (approx_len <= FT(0)) continue;

        FT disp = delta_max / approx_len;
        mesh.point(v) -= normal_sum * disp;
    }
}

/**
 * separate_kissing_columns:
 * Domain-agnostic 3D manifold repair that resolves zero-volume contact singularities
 * (kissing edges, kissing curves, and forking arms) using localized inward clearance collars.
 */
template <typename K = EK>
bool separate_kissing_columns(
    CGAL::Surface_mesh<typename K::Point_3>& mesh, 
    typename K::FT delta_max = 0.01,
    typename K::FT collar_len = 0.01
) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename K::Point_3 Point_3;
    typedef typename K::FT FT;

    std::map<Point_3, std::vector<Vertex_index>> coord_map;
    for (auto v : mesh.vertices()) coord_map[mesh.point(v)].push_back(v);

    std::vector<Vertex_index> colliding_verts;
    for (const auto& [pt, vs] : coord_map) {
        if (vs.size() > 1) {
            for (auto v : vs) colliding_verts.push_back(v);
        }
    }
    if (colliding_verts.empty()) return false;

    // Stage 1: Insert transition collars on edges rising from single-vertex base anchors
    auto collar_verts = insert_transition_collars(mesh, coord_map, collar_len);

    // Stage 2: Retract all contact and collar vertices inward into solid interior
    std::set<Vertex_index> verts_to_retract(colliding_verts.begin(), colliding_verts.end());
    verts_to_retract.insert(collar_verts.begin(), collar_verts.end());
    retract_contact_vertices(mesh, verts_to_retract, delta_max);

    if (!collar_verts.empty()) {
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    }
    mesh.collect_garbage();
    return true;
}

} // namespace fix
} // namespace geo
} // namespace jotcad
