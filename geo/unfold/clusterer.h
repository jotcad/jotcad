#pragma once

#include "types.h"
#include "flattener.h"
#include <queue>
#include <set>
#include <algorithm>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

namespace jotcad {
namespace geo {
namespace unfold {

using Mesh = boolean::ExactMesh;
using Face = Mesh::Face_index;
using Halfedge = Mesh::Halfedge_index;

class Clusterer {
public:
    static std::vector<UnfoldPatch> unfold(const Mesh& mesh) {
        // 1. Phase 1: Group Coplanar Faces into "Atoms"
        std::vector<UnfoldPatch> patches;
        std::set<Face> visited;
        
        for (auto f : mesh.faces()) {
            if (visited.count(f)) continue;

            UnfoldPatch patch;
            patch.tf = Flattener::get_face_transform(mesh, f);
            
            std::queue<Face> queue;
            queue.push(f);
            visited.insert(f);

            while (!queue.empty()) {
                Face fi = queue.front();
                queue.pop();
                
                patch.face_indices.push_back((size_t)fi);
                
                boolean::Polygon_2 poly;
                auto h_start = mesh.halfedge(fi);
                auto h = h_start;
                do {
                    auto p3d = mesh.point(mesh.source(h));
                    auto lp = patch.tf.transform(p3d);
                    poly.push_back(EK::Point_2(lp.x(), lp.y()));
                    h = mesh.next(h);
                } while (h != h_start);
                
                if (poly.is_clockwise_oriented()) poly.reverse_orientation();
                patch.gps.join(poly);

                h = h_start;
                do {
                    Halfedge hj = mesh.opposite(h);
                    if (!mesh.is_border(hj)) {
                        Face fj = mesh.face(hj);
                        if (Flattener::is_coplanar(mesh, fi, fj)) {
                            if (!visited.count(fj)) {
                                visited.insert(fj);
                                queue.push(fj);
                            }
                        } else {
                            add_boundary_handle(mesh, h, patch);
                        }
                    } else {
                        add_boundary_handle(mesh, h, patch);
                    }
                    h = mesh.next(h);
                } while (h != h_start);
            }
            patches.push_back(std::move(patch));
        }

        // 2. Phase 2: Jigsaw Merge
        std::vector<size_t> parent(patches.size());
        for (size_t i = 0; i < patches.size(); ++i) parent[i] = i;
        auto find = [&](auto self, size_t i) -> size_t {
            return (parent[i] == i) ? i : (parent[i] = self(self, parent[i]));
        };

        struct Hinge {
            size_t patch1, patch2;
            size_t h1, h2;
            double distance_sq;
        };
        std::vector<Hinge> hinges;
        for (size_t i = 0; i < patches.size(); ++i) {
            for (auto const& [h_idx, seg] : patches[i].boundary_halfedges) {
                Halfedge h1 = Halfedge(h_idx);
                Halfedge h2 = mesh.opposite(h1);
                if (mesh.is_border(h2)) continue;
                
                Face f2 = mesh.face(h2);
                for (size_t j = 0; j < patches.size(); ++j) {
                    if (i == j) continue;
                    bool owns_f2 = false;
                    for (size_t f_idx : patches[j].face_indices) {
                        if (f_idx == (size_t)f2) { owns_f2 = true; break; }
                    }
                    if (owns_f2) {
                        if (i < j) {
                            auto p1 = mesh.point(mesh.source(h1));
                            double d2 = CGAL::to_double((p1 - EK::Point_3(0,0,0)).squared_length());
                            hinges.push_back({i, j, (size_t)h1, (size_t)h2, d2});
                        }
                        break;
                    }
                }
            }
        }
        
        std::sort(hinges.begin(), hinges.end(), [](const Hinge& a, const Hinge& b) {
            return a.distance_sq < b.distance_sq;
        });

        for (const auto& hinge : hinges) {
            size_t root1 = find(find, hinge.patch1);
            size_t root2 = find(find, hinge.patch2);
            if (root1 == root2) continue;

            UnfoldPatch& p1 = patches[root1];
            UnfoldPatch& p2 = patches[root2];

            auto seg1 = p1.boundary_halfedges.at(hinge.h1);
            auto seg2 = p2.boundary_halfedges.at(hinge.h2);
            
            Matrix snap_tf = Flattener::get_2d_snap_transform(seg1.first, seg1.second, seg2.second, seg2.first);
            
            boolean::General_polygon_set_2 p2_gps_transformed = p2.gps;
            Flattener::transform_gps(p2_gps_transformed, snap_tf);

            if (p1.gps.do_intersect(p2_gps_transformed)) continue;

            p1.gps.join(p2_gps_transformed);
            p1.face_indices.insert(p1.face_indices.end(), p2.face_indices.begin(), p2.face_indices.end());
            
            p1.edge_tags[{std::min((int)mesh.source(Halfedge(hinge.h1)), (int)mesh.target(Halfedge(hinge.h1))), 
                         std::max((int)mesh.source(Halfedge(hinge.h1)), (int)mesh.target(Halfedge(hinge.h1)))}] = "fold";
            p1.fold_segments.push_back({seg1.first, seg1.second});
            
            for (auto const& fold : p2.fold_segments) {
                p1.fold_segments.push_back({snap_tf.transform_2d(fold.first), snap_tf.transform_2d(fold.second)});
            }

            for (auto const& [h_idx, seg] : p2.boundary_halfedges) {
                if (h_idx == hinge.h2) continue;
                p1.boundary_halfedges[h_idx] = {snap_tf.transform_2d(seg.first), snap_tf.transform_2d(seg.second)};
            }
            p1.boundary_halfedges.erase(hinge.h1);

            parent[root2] = root1;
        }

        std::vector<UnfoldPatch> final_patches;
        for (size_t i = 0; i < patches.size(); ++i) {
            if (parent[i] == i) {
                finalize_patch(patches[i]);
                final_patches.push_back(std::move(patches[i]));
            }
        }
        return final_patches;
    }

private:
    static void add_boundary_handle(const Mesh& mesh, Halfedge h, UnfoldPatch& patch) {
        auto pU = mesh.point(mesh.source(h));
        auto pV = mesh.point(mesh.target(h));
        auto lpU = patch.tf.transform(pU);
        auto lpV = patch.tf.transform(pV);
        patch.boundary_halfedges[(size_t)h] = {EK::Point_2(lpU.x(), lpU.y()), EK::Point_2(lpV.x(), lpV.y())};
    }

    static void finalize_patch(UnfoldPatch& patch) {
        std::vector<boolean::Polygon_with_holes_2> pwhs;
        patch.gps.polygons_with_holes(std::back_inserter(pwhs));
        patch.geometry.vertices.clear();
        patch.geometry.faces.clear();
        for (const auto& pwh : pwhs) {
            Geometry::Face face;
            auto process_loop = [&](const boolean::Polygon_2& poly) {
                std::vector<int> indices;
                for (auto it = poly.vertices_begin(); it != poly.vertices_end(); ++it) {
                    indices.push_back((int)patch.geometry.vertices.size());
                    patch.geometry.vertices.push_back(Vertex{it->x(), it->y(), FT(0)});
                }
                return indices;
            };
            face.loops.push_back(process_loop(pwh.outer_boundary()));
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                face.loops.push_back(process_loop(*hit));
            }
            patch.geometry.faces.push_back(std::move(face));
        }
    }
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
