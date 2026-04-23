#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <vector>
#include <algorithm>
#include "../impl/kernel.h"

namespace jotcad {
namespace geo {
namespace fix {

/**
 * repair_self_touches: Implements Umbrella Splitting & Geometric Locking.
 * 
 * This function identifies non-manifold vertices (bowties), splits the combinatorial
 * umbrellas into separate vertices, subdivides the local neighborhood to preserve
 * planarity, and displaces the resulting apices to ensure geometric separation.
 */
template <typename K = EK>
bool repair_self_touches(CGAL::Surface_mesh<typename K::Point_3>& mesh, typename K::FT delta = 0.0001) {
    typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
    typedef typename Surface_mesh::Vertex_index Vertex_index;
    typedef typename Surface_mesh::Halfedge_index Halfedge_index;
    typedef typename K::Point_3 Point_3;
    typedef typename K::Vector_3 Vector_3;

    // Step 1 & 2: Combinatorial Splitting
    std::vector<std::vector<Vertex_index>> vertex_groups;
    CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(
        mesh,
        CGAL::parameters::output_iterator(std::back_inserter(vertex_groups))
    );

    if (vertex_groups.empty()) return false;

    for (auto& group : vertex_groups) {
        for (Vertex_index v : group) {
            // Step 3: Local Subdivision
            std::vector<Halfedge_index> outgoing_halfedges;
            Halfedge_index h = mesh.halfedge(v);
            if (h == Surface_mesh::null_halfedge()) continue;

            // Collect all outgoing halfedges around the vertex
            Halfedge_index start = h;
            do {
                outgoing_halfedges.push_back(h);
                h = mesh.next_around_target(h);
            } while (h != start);

            // Calculate local subdivision scale (safety clamp)
            double local_delta = CGAL::to_double(delta);
            for (Halfedge_index eh : outgoing_halfedges) {
                double len = std::sqrt(CGAL::to_double(CGAL::squared_distance(mesh.point(mesh.source(eh)), mesh.point(mesh.target(eh)))));
                if (len > 0) local_delta = (std::min)(local_delta, len / 10.0);
            }

            Vector_3 umbrella_sum(0, 0, 0);
            std::vector<Halfedge_index> split_halfedges;

            for (Halfedge_index eh : outgoing_halfedges) {
                Point_3 p_source = mesh.point(mesh.source(eh));
                Point_3 p_target = mesh.point(mesh.target(eh));
                Vector_3 vec = p_source - p_target; // Vector pointing towards V
                
                double dist = std::sqrt(CGAL::to_double(vec.squared_length()));
                if (dist < 1e-12) continue;

                // Subdivide the edge close to V
                Halfedge_index new_h = CGAL::Euler::split_edge(eh, mesh);
                
                // Vector scaling in double then convert back to FT
                double scale = local_delta / dist;
                Vector_3 offset(typename K::FT(CGAL::to_double(vec.x()) * scale),
                                typename K::FT(CGAL::to_double(vec.y()) * scale),
                                typename K::FT(CGAL::to_double(vec.z()) * scale));
                
                mesh.point(mesh.target(new_h)) = p_target + offset;
                umbrella_sum += offset;
                split_halfedges.push_back(new_h);
            }

            // Patch the faces between split points
            for (size_t i = 0; i < split_halfedges.size(); ++i) {
                Halfedge_index h1 = split_halfedges[i];
                Halfedge_index h2 = split_halfedges[(i + 1) % split_halfedges.size()];
                if (mesh.face(h1) != Surface_mesh::null_face() && mesh.face(h1) == mesh.face(h2)) {
                    CGAL::Euler::split_face(h1, h2, mesh);
                }
            }

            // Step 4: Geometric Perturbation (Locking)
            Vector_3 average_offset = umbrella_sum / typename K::FT(split_halfedges.size());
            mesh.point(v) += average_offset / 2.0;
        }
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    return true;
}

} // namespace fix
} // namespace geo
} // namespace jotcad
