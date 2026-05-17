#pragma once

#include <queue>
#include <set>
#include <map>
#include <iostream>
#include "types.h"
#include "flattener.h"

namespace jotcad {
namespace geo {
namespace unfold {

class Clusterer {
public:
    typedef boolean::Surface_mesh Mesh;
    typedef Mesh::Face_index Face;
    typedef Mesh::Halfedge_index Halfedge;
    typedef Mesh::Vertex_index Vertex;

    static std::vector<UnfoldPatch> unfold(const Mesh& mesh) {
        std::vector<UnfoldPatch> patches;
        std::set<Face> visited;

        for (auto f_start : mesh.faces()) {
            if (visited.count(f_start)) continue;

            UnfoldPatch patch;
            EK::Plane_3 island_plane = Flattener::get_face_plane(mesh, f_start);
            
            std::queue<Face> queue;
            queue.push(f_start);
            visited.insert(f_start);

            while (!queue.empty()) {
                Face fi = queue.front();
                queue.pop();

                add_face_to_patch_plane(mesh, fi, island_plane, patch);

                auto h_start = mesh.halfedge(fi);
                auto h = h_start;
                do {
                    auto hj = mesh.opposite(h);
                    if (!mesh.is_border(hj)) {
                        Face fj = mesh.face(hj);
                        if (!visited.count(fj)) {
                            if (Flattener::is_coplanar(mesh, fi, fj)) {
                                visited.insert(fj);
                                queue.push(fj);
                                patch.edge_tags[sort_edge(mesh, h)] = "fold";
                            } else {
                                patch.edge_tags[sort_edge(mesh, h)] = "cut";
                            }
                        } else {
                            patch.edge_tags[sort_edge(mesh, h)] = "fold";
                        }
                    }
                    h = mesh.next(h);
                } while (h != h_start);
            }
            
            finalize_patch(mesh, patch);
            patches.push_back(std::move(patch));
        }

        return patches;
    }

private:
    static std::pair<int, int> sort_edge(const Mesh& mesh, Halfedge h) {
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        return {std::min(u, v), std::max(u, v)};
    }

    static void add_face_to_patch_plane(const Mesh& mesh, Face f, const EK::Plane_3& plane, UnfoldPatch& patch) {
        patch.face_indices.push_back((size_t)f);
        
        boolean::Polygon_2 poly;
        auto h_start = mesh.halfedge(f);
        auto h = h_start;
        do {
            auto p3d = mesh.point(mesh.source(h));
            auto p2d = plane.to_2d(p3d);
            poly.push_back(p2d);
            h = mesh.next(h);
        } while (h != h_start);

        if (poly.size() < 3) return;

        if (poly.is_clockwise_oriented()) {
            poly.reverse_orientation();
        }
        
        patch.gps.join(poly);
    }

    static void finalize_patch(const Mesh& mesh, UnfoldPatch& patch) {
        // Convert the merged GPS into Geometry loops
        std::vector<boolean::Polygon_with_holes_2> pwhs;
        patch.gps.polygons_with_holes(std::back_inserter(pwhs));

        patch.geometry.vertices.clear();
        patch.geometry.faces.clear();
        
        // We'll regenerate edge_tags to refer to the NEW 2D vertex indices
        // but only for the boundaries (cuts). 
        // Internal folds are harder to recover from the GPS, so we'll 
        // keep the original ones from the growth loop if they exist.
        std::map<std::pair<int, int>, std::string> new_tags;

        for (const auto& pwh : pwhs) {
            Geometry::Face face;
            
            auto process_loop = [&](const boolean::Polygon_2& poly) {
                std::vector<int> indices;
                int start_idx = (int)patch.geometry.vertices.size();
                for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                    indices.push_back((int)patch.geometry.vertices.size());
                    patch.geometry.vertices.push_back({it->x(), it->y(), FT(0)});
                }
                for (size_t i = 0; i < indices.size(); ++i) {
                    int u = indices[i];
                    int v = indices[(i + 1) % indices.size()];
                    new_tags[{std::min(u, v), std::max(u, v)}] = "cut";
                }
                return indices;
            };

            face.loops.push_back(process_loop(pwh.outer_boundary()));
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                face.loops.push_back(process_loop(*hit));
            }
            patch.geometry.faces.push_back(std::move(face));
        }
        
        // Replace old tags with new boundary tags
        patch.edge_tags = std::move(new_tags);
    }
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
