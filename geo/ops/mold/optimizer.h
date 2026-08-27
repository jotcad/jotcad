#pragma once
#include "types.h"
#include <cmath>
#include <CGAL/Polygon_mesh_processing/border.h>

namespace jotcad {
namespace geo {
namespace mold {

struct PartingOptimizationResult {
    EK::Vector_3 best_dir;
    std::vector<ExactMesh::Face_index> positive_patch_faces;
    std::vector<std::pair<int, int>> boundary_cycle;
    int cycle_count;
};

inline PartingOptimizationResult optimize_parting_direction(
    const ExactMesh& mesh_part,
    const std::vector<EK::Vector_3>& face_normals,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceBoolMap is_handled
) {
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
    int best_segment_count = 999999;
    std::vector<ExactMesh::Face_index> best_patch_faces;
    std::vector<std::pair<int, int>> best_boundary_cycle;

    std::vector<ExactMesh::Face_index> face_descriptors;
    for (auto f : CGAL::faces(mesh_part)) {
        face_descriptors.push_back(f);
    }

    for (const auto& d : candidate_dirs) {
        std::vector<ExactMesh::Face_index> current_patch_faces;
        for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
            auto f = face_descriptors[f_idx];
            if (is_handled[f]) continue;
            if (face_normals[f_idx] * d > FT(0)) {
                current_patch_faces.push_back(f);
            }
        }
        if (current_patch_faces.empty() || current_patch_faces.size() == face_descriptors.size()) continue;

        std::vector<ExactMesh::Halfedge_index> border_halfedges;
        CGAL::Polygon_mesh_processing::border_halfedges(
            current_patch_faces, mesh_part, std::back_inserter(border_halfedges)
        );
        if (border_halfedges.empty()) continue;

        // Group border halfedges into connected cycles
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
            int seg_count = (int)cycles[0].size();
            if (best_loop_count > 1 || seg_count < best_segment_count) {
                best_loop_count = 1;
                best_segment_count = seg_count;
                best_dir = d;
                best_patch_faces = current_patch_faces;
                best_boundary_cycle = cycles[0];
            }
        } else if (best_loop_count > 1 && cycle_count > 0 && cycle_count < best_loop_count) {
            best_loop_count = cycle_count;
            best_dir = d;
            best_patch_faces = current_patch_faces;
            best_boundary_cycle = cycles.empty() ? std::vector<std::pair<int,int>>() : cycles[0];
        }
    }

    if (best_loop_count > 1 || best_boundary_cycle.empty()) {
        throw std::runtime_error("Demoldability Error: Geometry contains internal undercut islands along all tested draw vectors (requires multi-stage side lifters).");
    }

    return {best_dir, best_patch_faces, best_boundary_cycle, best_loop_count};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
