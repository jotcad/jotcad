#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <vector>
#include <algorithm>
#include <map>
#include "../impl/kernel.h"

namespace jotcad {
namespace geo {
namespace fix {

/**
 * is_geometry_unambiguous: 
 * Returns true if the mesh is topologically manifold AND its manifoldness is 
 * "stable" against spatial merging.
 */
template <typename K = EK>
bool is_geometry_unambiguous(const CGAL::Surface_mesh<typename K::Point_3>& mesh) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename K::Point_3 Point_3;

    // 1. Immediate Non-Manifold Check
    std::vector<typename Surface_mesh::Halfedge_index> h_nm;
    CGAL::Polygon_mesh_processing::non_manifold_vertices(mesh, std::back_inserter(h_nm));
    if (!h_nm.empty()) return false;

    // 2. Latent Non-Manifold Check (Spatial Collisions)
    std::vector<Point_3> points;
    std::vector<std::vector<std::size_t>> faces;
    
    std::map<Vertex_index, std::size_t> v_to_idx;
    for (auto v : mesh.vertices()) {
        v_to_idx[v] = points.size();
        points.push_back(mesh.point(v));
    }
    for (auto f : mesh.faces()) {
        std::vector<std::size_t> face;
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
            face.push_back(v_to_idx[v]);
        }
        faces.push_back(face);
    }

    CGAL::Polygon_mesh_processing::repair_polygon_soup(points, faces);

    Surface_mesh merged_mesh;
    std::vector<Vertex_index> merged_vs;
    for (const auto& pt : points) merged_vs.push_back(merged_mesh.add_vertex(pt));
    for (const auto& f_indices : faces) {
        std::vector<Vertex_index> face_vs;
        for (auto idx : f_indices) face_vs.push_back(merged_vs[idx]);
        if (merged_mesh.add_face(face_vs) == Surface_mesh::null_face()) {
            return false; 
        }
    }

    std::vector<typename Surface_mesh::Halfedge_index> h_nm_merged;
    CGAL::Polygon_mesh_processing::non_manifold_vertices(merged_mesh, std::back_inserter(h_nm_merged));
    return h_nm_merged.empty();
}

/**
 * make_geometry_unambiguous:
 * Resolves both actual and latent non-manifold singularities.
 */
template <typename K = EK>
bool make_geometry_unambiguous(CGAL::Surface_mesh<typename K::Point_3>& mesh, typename K::FT delta = 0.0001) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename Surface_mesh::Halfedge_index Halfedge_index;
    typedef typename K::Point_3 Point_3;
    typedef typename K::Vector_3 Vector_3;

    bool changed = false;

    // Phase 1: Handle Combinatorial Non-Manifoldness (Already merged indices)
    std::vector<std::vector<Vertex_index>> vertex_groups;
    CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(
        mesh,
        CGAL::parameters::output_iterator(std::back_inserter(vertex_groups))
    );

    if (!vertex_groups.empty()) {
        changed = true;
        for (auto& group : vertex_groups) {
            for (Vertex_index v : group) {
                std::vector<Halfedge_index> outgoing;
                Halfedge_index h = mesh.halfedge(v);
                if (h == Surface_mesh::null_halfedge()) continue;
                Halfedge_index start = h;
                do {
                    outgoing.push_back(h);
                    h = mesh.next_around_target(h);
                } while (h != start);

                double local_delta = CGAL::to_double(delta);
                Vector_3 umbrella_sum(0, 0, 0);
                std::vector<Halfedge_index> splits;
                for (auto eh : outgoing) {
                    Point_3 p_s = mesh.point(mesh.source(eh)), p_t = mesh.point(mesh.target(eh));
                    Vector_3 vec = p_s - p_t; 
                    double dist = std::sqrt(CGAL::to_double(vec.squared_length()));
                    if (dist < 1e-12) continue;
                    auto new_h = CGAL::Euler::split_edge(eh, mesh);
                    double scale = local_delta / dist;
                    Vector_3 offset(typename K::FT(CGAL::to_double(vec.x()) * scale),
                                    typename K::FT(CGAL::to_double(vec.y()) * scale),
                                    typename K::FT(CGAL::to_double(vec.z()) * scale));
                    mesh.point(mesh.target(new_h)) = p_t + offset;
                    umbrella_sum += offset;
                    splits.push_back(new_h);
                }
                for (size_t i = 0; i < splits.size(); ++i) {
                    auto h1 = splits[i], h2 = splits[(i + 1) % splits.size()];
                    if (mesh.face(h1) != Surface_mesh::null_face() && mesh.face(h1) == mesh.face(h2)) 
                        CGAL::Euler::split_face(h1, h2, mesh);
                }
                mesh.point(v) += (umbrella_sum / typename K::FT(splits.size())) / 2.0;
            }
        }
    }

    // Phase 2: Handle Latent Non-Manifoldness (Coordinate collisions)
    std::map<Point_3, std::vector<Vertex_index>> coord_map;
    for (auto v : mesh.vertices()) coord_map[mesh.point(v)].push_back(v);

    for (auto const& [pt, vs] : coord_map) {
        if (vs.size() > 1) {
            changed = true;
            for (auto v : vs) {
                std::vector<Halfedge_index> outgoing;
                Halfedge_index h = mesh.halfedge(v);
                if (h == Surface_mesh::null_halfedge()) continue;
                Halfedge_index start = h;
                do {
                    outgoing.push_back(h);
                    h = mesh.next_around_target(h);
                } while (h != start);

                double local_delta = CGAL::to_double(delta);
                Vector_3 umbrella_sum(0, 0, 0);
                std::vector<Halfedge_index> splits;
                for (auto eh : outgoing) {
                    Point_3 p_s = mesh.point(mesh.source(eh)), p_t = mesh.point(mesh.target(eh));
                    Vector_3 vec = p_s - p_t;
                    double dist = std::sqrt(CGAL::to_double(vec.squared_length()));
                    if (dist < 1e-12) continue;
                    auto new_h = CGAL::Euler::split_edge(eh, mesh);
                    double scale = local_delta / dist;
                    Vector_3 offset(typename K::FT(CGAL::to_double(vec.x()) * scale),
                                    typename K::FT(CGAL::to_double(vec.y()) * scale),
                                    typename K::FT(CGAL::to_double(vec.z()) * scale));
                    mesh.point(mesh.target(new_h)) = p_t + offset;
                    umbrella_sum += offset;
                    splits.push_back(new_h);
                }
                for (size_t i = 0; i < splits.size(); ++i) {
                    auto h1 = splits[i], h2 = splits[(i+1)%splits.size()];
                    if (mesh.face(h1) != Surface_mesh::null_face() && mesh.face(h1) == mesh.face(h2)) 
                        CGAL::Euler::split_face(h1, h2, mesh);
                }
                mesh.point(v) += (umbrella_sum / typename K::FT(splits.size())) / 2.0;
            }
        }
    }

    if (changed) CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    return changed;
}

} // namespace fix
} // namespace geo
} // namespace jotcad
