#pragma once
#include "types.h"
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

namespace jotcad {
namespace geo {
namespace mold {

inline ExactMesh normalize_and_repair_solid(const Geometry& world_geo) {
    ExactMesh mesh = boolean::Engine::geometry_to_mesh(world_geo);
    CGAL::Polygon_mesh_processing::stitch_borders(mesh);
    CGAL::Polygon_mesh_processing::remove_almost_degenerate_faces(mesh);
    CGAL::Polygon_mesh_processing::remove_isolated_vertices(mesh);

    if (!CGAL::is_closed(mesh)) {
        std::vector<ExactMesh::Halfedge_index> borders;
        for (auto h : mesh.halfedges()) {
            if (mesh.is_border(h)) borders.push_back(h);
        }
        for (auto h : borders) {
            if (mesh.is_border(h)) {
                std::vector<ExactMesh::Vertex_index> hole_vs;
                auto curr = h;
                do {
                    hole_vs.push_back(mesh.target(curr));
                    curr = mesh.next(curr);
                } while (curr != h && hole_vs.size() < 1000);
                if (hole_vs.size() >= 3) {
                    auto v0 = hole_vs[0];
                    for (size_t i = 1; i + 1 < hole_vs.size(); ++i) {
                        mesh.add_face(v0, hole_vs[i], hole_vs[i+1]);
                    }
                }
            }
        }
    }
    mesh.collect_garbage();
    return mesh;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
