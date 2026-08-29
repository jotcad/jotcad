#pragma once
#include "types.h"
#include "visibility.h"
#include <cmath>
#include <CGAL/Polygon_mesh_processing/border.h>

namespace jotcad {
namespace geo {
namespace mold {

struct PartingOptimizationResult {
    EK::Vector_3 best_dir;
    ExactMesh solid_wedge;
    std::set<size_t> source_faces;
    int cycle_count;
};

inline PartingOptimizationResult optimize_parting_direction(
    const ExactMesh& mesh_part,
    const std::vector<EK::Vector_3>& face_normals,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceBoolMap is_handled,
    const MoldParams& params
) {
    FT min_dot(std::sin(CGAL::to_double(params.draft) * 2.0 * M_PI));

    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<FT> face_areas;
    face_descriptors.reserve(mesh_part.number_of_faces());
    face_areas.reserve(mesh_part.number_of_faces());

    for (auto f : CGAL::faces(mesh_part)) {
        face_descriptors.push_back(f);
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        face_areas.push_back(std::sqrt(CGAL::to_double(CGAL::squared_area(p0, p1, p2))));
    }

    std::vector<EK::Vector_3> candidate_dirs;
    const int N = 500;
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    for (int i = 0; i < N; ++i) {
        double y = 1.0 - (i / double(N - 1)) * 2.0;
        double radius = std::sqrt(std::max(0.0, 1.0 - y * y));
        double theta = 2.0 * M_PI * i / phi;
        double x = std::cos(theta) * radius;
        double z = std::sin(theta) * radius;
        candidate_dirs.push_back(EK::Vector_3(FT(x), FT(y), FT(z)));
    }
    candidate_dirs.push_back(EK::Vector_3(FT(1), FT(0), FT(0)));
    candidate_dirs.push_back(EK::Vector_3(FT(-1), FT(0), FT(0)));
    candidate_dirs.push_back(EK::Vector_3(FT(0), FT(1), FT(0)));
    candidate_dirs.push_back(EK::Vector_3(FT(0), FT(-1), FT(0)));
    candidate_dirs.push_back(EK::Vector_3(FT(0), FT(0), FT(1)));
    candidate_dirs.push_back(EK::Vector_3(FT(0), FT(0), FT(-1)));

    EK::Vector_3 best_dir(FT(1), FT(0), FT(0));
    int best_loop_count = 999999;
    FT best_patch_area = -1;
    std::vector<ExactMesh::Face_index> best_patch_faces;

    std::cout << "    [Optimizer] Scanning " << candidate_dirs.size() << " candidate directions..." << std::flush;
    auto t_opt_start = std::chrono::steady_clock::now();

    for (const auto& d : candidate_dirs) {
        auto visible_faces = compute_visible_patch_faces_fast(
            mesh_part, face_descriptors, face_normals, is_handled, d, min_dot
        );
        if (visible_faces.empty()) continue;

        // Group visible_faces into connected components via edge_to_faces
        std::map<ExactMesh::Face_index, int> face_to_local;
        for (size_t i = 0; i < visible_faces.size(); ++i) {
            face_to_local[visible_faces[i]] = (int)i;
        }

        DSU patch_dsu((int)visible_faces.size());
        for (const auto& [edge, faces] : edge_to_faces) {
            std::vector<int> visible_in_edge;
            for (int f_idx : faces) {
                auto f = face_descriptors[f_idx];
                auto it = face_to_local.find(f);
                if (it != face_to_local.end()) {
                    visible_in_edge.push_back(it->second);
                }
            }
            if (visible_in_edge.size() >= 2) {
                for (size_t i = 1; i < visible_in_edge.size(); ++i) {
                    patch_dsu.unite(visible_in_edge[0], visible_in_edge[i]);
                }
            }
        }

        std::map<int, std::vector<ExactMesh::Face_index>> components;
        for (size_t i = 0; i < visible_faces.size(); ++i) {
            int root = patch_dsu.find((int)i);
            components[root].push_back(visible_faces[i]);
        }

        std::vector<ExactMesh::Face_index> largest_comp;
        FT current_patch_area = 0;
        for (const auto& [root, comp_faces] : components) {
            FT comp_area = 0;
            for (auto f : comp_faces) {
                comp_area += face_areas[(size_t)f];
            }
            if (comp_area > current_patch_area) {
                current_patch_area = comp_area;
                largest_comp = comp_faces;
            }
        }

        std::vector<ExactMesh::Halfedge_index> border_halfedges;
        CGAL::Polygon_mesh_processing::border_halfedges(
            largest_comp, mesh_part, std::back_inserter(border_halfedges)
        );

        std::map<int, int> next_v;
        for (auto h : border_halfedges) {
            next_v[(int)mesh_part.source(h)] = (int)mesh_part.target(h);
        }

        std::set<int> visited;
        std::vector<std::vector<std::pair<int, int>>> cycles;
        for (auto h : border_halfedges) {
            int start = (int)mesh_part.source(h);
            if (visited.count(start)) continue;

            std::vector<std::pair<int, int>> current_cycle;
            int curr = start;
            while (curr != -1 && !visited.count(curr)) {
                visited.insert(curr);
                auto it = next_v.find(curr);
                if (it == next_v.end()) break;
                int nxt = it->second;
                current_cycle.push_back({curr, nxt});
                curr = nxt;
                if (curr == start) break;
            }
            if (curr == start && current_cycle.size() >= 3) {
                cycles.push_back(current_cycle);
            }
        }

        int cycle_count = (int)cycles.size();
        if (cycle_count == 1) {
            if (best_loop_count > 1 || current_patch_area > best_patch_area) {
                best_loop_count = 1;
                best_patch_area = current_patch_area;
                best_dir = d;
                best_patch_faces = largest_comp;
            }
        } else if (best_loop_count > 1 && cycle_count > 0 && (best_patch_area < 0 || current_patch_area > best_patch_area)) {
            best_loop_count = cycle_count;
            best_patch_area = current_patch_area;
            best_dir = d;
            best_patch_faces = largest_comp;
        }
    }

    auto t_opt_end = std::chrono::steady_clock::now();
    double opt_ms = std::chrono::duration<double, std::milli>(t_opt_end - t_opt_start).count();
    std::cout << " Done in " << opt_ms << "ms. Best dir: (" 
              << CGAL::to_double(best_dir.x()) << ", " << CGAL::to_double(best_dir.y()) << ", " << CGAL::to_double(best_dir.z())
              << ") with " << best_patch_faces.size() << " seed faces." << std::endl << std::flush;

    if (best_loop_count > 1) {
        throw std::runtime_error("Demoldability Error: Geometry contains internal undercut islands along all tested draw vectors (requires multi-stage side lifters).");
    }

    // Compute exact Upper Envelope mesh along best_dir within the OBB corridor of best_patch_faces
    auto env_res = compute_exact_upper_envelope_mesh(
        mesh_part, face_descriptors, face_normals, is_handled, best_dir, min_dot, best_patch_faces
    );

    if (env_res.solid_wedge.number_of_faces() == 0) {
        return {best_dir, {}, {}, 0};
    }

    return {best_dir, env_res.solid_wedge, env_res.source_faces, 1};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
