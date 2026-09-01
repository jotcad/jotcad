#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include "kernel.h"

namespace jotcad {
namespace geo {
namespace fix {

/**
 * bridge_zero_volume_touches:
 * Strategy II from docs/TOPOLOGICAL_SINGULARITIES.md.
 * 
 * Fuses coincident touching apexes/umbrellas into a single continuous 2-manifold
 * solid by inserting positive-volume bridge facets across a throat of width delta.
 */
template <typename K = EK>
bool bridge_zero_volume_touches(CGAL::Surface_mesh<typename K::Point_3>& mesh, typename K::FT delta = 0.01) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename Surface_mesh::Halfedge_index Halfedge_index;
    typedef typename K::Point_3 Point_3;
    typedef typename K::Vector_3 Vector_3;

    // 1. Group vertices by exact spatial coordinates
    std::map<Point_3, std::vector<Vertex_index>> coord_map;
    for (auto v : mesh.vertices()) {
        coord_map[mesh.point(v)].push_back(v);
    }

    bool changed = false;
    double local_delta = CGAL::to_double(delta);

    for (const auto& [pt, vs] : coord_map) {
        if (vs.size() < 2) continue;

        // Collect boundary outgoing halfedges for each touching vertex
        std::vector<std::pair<Vertex_index, std::vector<Halfedge_index>>> vertex_borders;
        for (auto v : vs) {
            std::vector<Halfedge_index> borders;
            Halfedge_index h = mesh.halfedge(v);
            if (h == Surface_mesh::null_halfedge()) continue;
            Halfedge_index start = h;
            do {
                if (mesh.is_border(h) || mesh.is_border(mesh.opposite(h))) {
                    borders.push_back(h);
                }
                h = mesh.next_around_target(h);
            } while (h != start);
            if (!borders.empty()) {
                vertex_borders.push_back({v, borders});
            }
        }

        if (vertex_borders.size() >= 2) {
            // Subdivide incident border edges at distance delta to create bridge anchor vertices
            std::vector<Vertex_index> bridge_anchors;
            for (const auto& [v, borders] : vertex_borders) {
                for (auto eh : borders) {
                    Point_3 p_s = mesh.point(mesh.source(eh));
                    Point_3 p_t = mesh.point(mesh.target(eh));
                    Vector_3 vec = p_s - p_t;
                    double dist = std::sqrt(CGAL::to_double(vec.squared_length()));
                    if (dist < 1e-9) continue;

                    double scale = (std::min)(local_delta, dist * 0.49) / dist;
                    auto new_h = CGAL::Euler::split_edge(eh, mesh);
                    Vector_3 offset(typename K::FT(CGAL::to_double(vec.x()) * scale),
                                    typename K::FT(CGAL::to_double(vec.y()) * scale),
                                    typename K::FT(CGAL::to_double(vec.z()) * scale));
                    auto new_v = mesh.target(new_h);
                    mesh.point(new_v) = p_t + offset;
                    bridge_anchors.push_back(new_v);
                }
            }

            // Insert bridge face connecting the anchor vertices
            if (bridge_anchors.size() >= 3) {
                auto new_face = mesh.add_face(bridge_anchors);
                if (new_face != Surface_mesh::null_face()) {
                    changed = true;
                }
            }
        }
    }

    if (changed) {
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    }
    return changed;
}

} // namespace fix
} // namespace geo
} // namespace jotcad
