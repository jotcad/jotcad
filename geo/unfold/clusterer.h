#pragma once

#include "types.h"
#include "flattener.h"
#include <queue>
#include <set>
#include <algorithm>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

#include <iostream>
#include <iomanip>
#include <chrono>

namespace jotcad {
namespace geo {
namespace unfold {

using Mesh = boolean::ExactMesh;
using Face = Mesh::Face_index;
using Halfedge = Mesh::Halfedge_index;

class Clusterer {
public:
    static std::vector<UnfoldPatch> unfold(const Mesh& mesh, double min_fold_deg = 1.0, std::string rule = "grow") {
        auto overall_start_time = std::chrono::high_resolution_clock::now();
        std::cout << "[Unfold] Starting unfold for mesh with " << mesh.number_of_faces() << " faces." << std::endl;
        std::cout << "[Unfold] Rule selected: " << rule << std::endl;

        // 1. Phase 1: Group Coplanar Faces into "Atoms"
        std::vector<UnfoldPatch> patches;
        std::set<Face> visited;
        std::map<Face, size_t> face_to_atom;
        
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
                face_to_atom[fi] = patches.size();
                
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

        size_t candidate_hinges_count = 0;
        for (auto e : mesh.edges()) {
            if (mesh.is_border(e)) continue;
            Halfedge h = mesh.halfedge(e, 0);
            Face f1 = mesh.face(h);
            Face f2 = mesh.face(mesh.opposite(h));
            if (face_to_atom[f1] != face_to_atom[f2]) {
                candidate_hinges_count++;
            }
        }

        std::cout << "[Unfold] Phase 1 complete: grouped " << mesh.number_of_faces() << " faces into " << patches.size() << " coplanar atoms." << std::endl;
        std::cout << "[Unfold] Predicted candidate hinges: " << candidate_hinges_count << std::endl;

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
            size_t size_sum;
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
                            hinges.push_back({i, j, (size_t)h1, (size_t)h2, d2, 2});
                        }
                        break;
                    }
                }
            }
        }

        auto start_time = std::chrono::high_resolution_clock::now();
        size_t processed = 0;
        size_t successful_merges = 0;
        size_t topological_cuts = 0;
        size_t intersection_skips = 0;

        auto print_progress = [&](size_t total_size) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = now - start_time;
            double elapsed_s = elapsed.count() / 1000.0;
            double avg_ms = (processed > 0) ? (elapsed.count() / processed) : 0.0;
            size_t remaining = (total_size > processed) ? (total_size - processed) : 0;
            double est_remaining_s = (remaining * avg_ms) / 1000.0;

            std::cout << "[Unfold] Progress: " << processed << "/" << total_size 
                      << " processed (" << remaining << " remaining). "
                      << "Merges: " << successful_merges 
                      << ", Cuts: " << topological_cuts 
                      << ", Intersection Skips: " << intersection_skips 
                      << " | Time: " << std::fixed << std::setprecision(2) << elapsed_s << "s"
                      << " (Avg: " << std::fixed << std::setprecision(1) << avg_ms << "ms/cand,"
                      << " Est. Remain: " << std::fixed << std::setprecision(2) << est_remaining_s << "s)" << std::endl;
        };

        auto apply_merge = [&](const Hinge& hinge) -> bool {
            size_t root1 = find(find, hinge.patch1);
            size_t root2 = find(find, hinge.patch2);
            if (root1 == root2) {
                topological_cuts++;
                return false;
            }

            UnfoldPatch& p1 = patches[root1];
            UnfoldPatch& p2 = patches[root2];

            auto seg1 = p1.boundary_halfedges.at(hinge.h1);
            auto seg2 = p2.boundary_halfedges.at(hinge.h2);
            
            Matrix snap_tf = Flattener::get_2d_snap_transform(seg1.first, seg1.second, seg2.second, seg2.first);
            
            boolean::General_polygon_set_2 p2_gps_transformed = p2.gps;
            Flattener::transform_gps(p2_gps_transformed, snap_tf);

            if (p1.gps.do_intersect(p2_gps_transformed)) {
                intersection_skips++;
                return false;
            }

            successful_merges++;
            p1.gps.join(p2_gps_transformed);
            p1.face_indices.insert(p1.face_indices.end(), p2.face_indices.begin(), p2.face_indices.end());
            
            auto h = Halfedge(hinge.h1);
            auto pt1 = mesh.point(mesh.source(h));
            auto pt2 = mesh.point(mesh.target(h));
            auto pt3 = mesh.point(mesh.target(mesh.next(h)));
            auto pt4 = mesh.point(mesh.target(mesh.next(mesh.opposite(h))));
            double angle_deg = std::abs(CGAL::to_double(CGAL::approximate_dihedral_angle(pt1, pt2, pt3, pt4)));
            double fold_deviation_deg = std::abs(180.0 - angle_deg);

            if (fold_deviation_deg >= min_fold_deg) {
                p1.edge_tags[{std::min((int)mesh.source(h), (int)mesh.target(h)), 
                             std::max((int)mesh.source(h), (int)mesh.target(h))}] = "fold";
                p1.fold_segments.push_back({seg1.first, seg1.second});
            }
            
            for (auto const& fold : p2.fold_segments) {
                p1.fold_segments.push_back({snap_tf.transform_2d(fold.first), snap_tf.transform_2d(fold.second)});
            }

            for (auto const& [h_idx, seg] : p2.boundary_halfedges) {
                if (h_idx == hinge.h2) continue;
                p1.boundary_halfedges[h_idx] = {snap_tf.transform_2d(seg.first), snap_tf.transform_2d(seg.second)};
            }
            p1.boundary_halfedges.erase(hinge.h1);

            parent[root2] = root1;
            return true;
        };

        if (rule == "pair") {
            auto compare = [&](const Hinge& a, const Hinge& b) {
                if (a.size_sum != b.size_sum) return a.size_sum > b.size_sum;
                return a.distance_sq > b.distance_sq;
            };
            std::priority_queue<Hinge, std::vector<Hinge>, decltype(compare)> pq(compare);
            for (const auto& h : hinges) pq.push(h);

            std::cout << "[Unfold] Phase 2: Processing Jigsaw Merge using 'pair' rule..." << std::endl;

            while (!pq.empty()) {
                Hinge hinge = pq.top();
                pq.pop();

                size_t root1 = find(find, hinge.patch1);
                size_t root2 = find(find, hinge.patch2);
                if (root1 == root2) {
                    processed++;
                    topological_cuts++;
                    if (candidate_hinges_count < 50 || processed % (candidate_hinges_count / 10 + 1) == 0) {
                        print_progress(candidate_hinges_count);
                    }
                    continue;
                }

                size_t current_size_sum = patches[root1].face_indices.size() + patches[root2].face_indices.size();
                if (current_size_sum > hinge.size_sum) {
                    Hinge updated = hinge;
                    updated.size_sum = current_size_sum;
                    pq.push(updated);
                    continue;
                }

                processed++;
                apply_merge(hinge);

                if (candidate_hinges_count < 50 || processed % (candidate_hinges_count / 10 + 1) == 0) {
                    print_progress(candidate_hinges_count);
                }
            }
        } else {
            std::sort(hinges.begin(), hinges.end(), [](const Hinge& a, const Hinge& b) {
                return a.distance_sq < b.distance_sq;
            });

            std::cout << "[Unfold] Phase 2: Processing Jigsaw Merge using 'grow' rule on " << hinges.size() << " candidate hinges..." << std::endl;

            for (const auto& hinge : hinges) {
                processed++;
                apply_merge(hinge);

                if (hinges.size() < 50 || processed % (hinges.size() / 10 + 1) == 0 || processed == hinges.size()) {
                    print_progress(hinges.size());
                }
            }
        }

        std::vector<UnfoldPatch> final_patches;
        double total_solid_area = 0.0;
        double total_bbox_area = 0.0;
        
        size_t actual_islands_count = 0;
        for (size_t i = 0; i < parent.size(); ++i) {
            if (parent[i] == i) actual_islands_count++;
        }
        std::cout << "[Unfold] Jigsaw complete. Generated " << actual_islands_count << " final island(s)." << std::endl;

        for (size_t i = 0; i < patches.size(); ++i) {
            if (parent[i] == i) {
                finalize_patch(patches[i]);
                
                std::vector<boolean::Polygon_with_holes_2> pwhs;
                patches[i].gps.polygons_with_holes(std::back_inserter(pwhs));
                double solid_area = 0.0;
                double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
                for (const auto& pwh : pwhs) {
                    double area = CGAL::to_double(pwh.outer_boundary().area());
                    solid_area += area;
                    for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                        double x = CGAL::to_double(it->x());
                        double y = CGAL::to_double(it->y());
                        if (x < min_x) min_x = x;
                        if (x > max_x) max_x = x;
                        if (y < min_y) min_y = y;
                        if (y > max_y) max_y = y;
                    }
                    for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                        solid_area -= std::abs(CGAL::to_double(hit->area()));
                        for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                            double x = CGAL::to_double(it->x());
                            double y = CGAL::to_double(it->y());
                            if (x < min_x) min_x = x;
                            if (x > max_x) max_x = x;
                            if (y < min_y) min_y = y;
                            if (y > max_y) max_y = y;
                        }
                    }
                }
                double w = max_x - min_x;
                double h = max_y - min_y;
                double bbox_area = (w > 0 && h > 0) ? (w * h) : 0.0;
                double wastage = (bbox_area > 0.0) ? (1.0 - (solid_area / bbox_area)) * 100.0 : 0.0;
                
                total_solid_area += solid_area;
                total_bbox_area += bbox_area;

                std::cout << "[Unfold] Island " << final_patches.size() << ": "
                          << patches[i].face_indices.size() << " faces, "
                          << "Solid Area: " << solid_area << " mm², "
                          << "Bounding Box: " << w << " x " << h << " (Area: " << bbox_area << " mm²), "
                          << "Wastage: " << std::fixed << std::setprecision(1) << wastage << "%" << std::endl;

                final_patches.push_back(std::move(patches[i]));
            }
        }
        
        double overall_wastage = (total_bbox_area > 0.0) ? (1.0 - (total_solid_area / total_bbox_area)) * 100.0 : 0.0;
        auto overall_end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> overall_elapsed = overall_end_time - overall_start_time;
        double total_s = overall_elapsed.count() / 1000.0;

        std::cout << "[Unfold] Unfolding complete. Total Islands: " << final_patches.size()
                  << ", Overall Wastage: " << std::fixed << std::setprecision(1) << overall_wastage << "%"
                  << " | Total Time: " << std::fixed << std::setprecision(3) << total_s << "s" << std::endl;

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
        patch.geometry.segments.clear();

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

        // Add fold line segments into the finalized patch geometry
        for (const auto& seg : patch.fold_segments) {
            int idx1 = (int)patch.geometry.vertices.size();
            patch.geometry.vertices.push_back(Vertex{seg.first.x(), seg.first.y(), FT(0)});
            int idx2 = (int)patch.geometry.vertices.size();
            patch.geometry.vertices.push_back(Vertex{seg.second.x(), seg.second.y(), FT(0)});
            patch.geometry.segments.push_back({idx1, idx2});
        }
    }
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
