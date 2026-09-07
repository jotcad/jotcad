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
    std::vector<FT> face_areas(mesh_part.num_faces());
    std::vector<EK::Point_3> face_centroids(mesh_part.num_faces());
    face_descriptors.reserve(mesh_part.number_of_faces());

    FT max_r_sq = 0;
    for (auto f : mesh_part.faces()) {
        face_descriptors.push_back(f);
        size_t f_idx = f.idx();
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        face_areas[f_idx] = std::sqrt(CGAL::to_double(CGAL::squared_area(p0, p1, p2)));
        EK::Point_3 c((p0.x() + p1.x() + p2.x()) / 3, (p0.y() + p1.y() + p2.y()) / 3, (p0.z() + p1.z() + p2.z()) / 3);
        face_centroids[f_idx] = c;
        FT d_sq = c.x()*c.x() + c.y()*c.y() + c.z()*c.z();
        if (d_sq > max_r_sq) max_r_sq = d_sq;
    }
    double r_bound = std::sqrt(CGAL::to_double(max_r_sq)) + 50.0;

    std::vector<EK::Vector_3> candidate_dirs;

    const int N = 300;
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    for (int i = 0; i < N; ++i) {
        double y = 1.0 - (i / double(N - 1)) * 2.0;
        double radius = std::sqrt(std::max(0.0, 1.0 - y * y));
        double theta = 2.0 * M_PI * i / phi;
        double x = std::cos(theta) * radius;
        double z = std::sin(theta) * radius;
        candidate_dirs.push_back(EK::Vector_3(FT(x), FT(y), FT(z)));
    }

    // Geometry-informed candidate directions:
    // 1. Vertex corner normals (pulling directly off sharp corners)
    for (auto v : mesh_part.vertices()) {
        EK::Vector_3 vn(0, 0, 0);
        for (auto f : CGAL::faces_around_target(mesh_part.halfedge(v), mesh_part)) {
            if (f != ExactMesh::null_face()) {
                vn = vn + face_normals[(size_t)f];
            }
        }
        double len = std::sqrt(CGAL::to_double(vn.squared_length()));
        if (len > 1e-6) {
            candidate_dirs.push_back(EK::Vector_3(FT(CGAL::to_double(vn.x()) / len), FT(CGAL::to_double(vn.y()) / len), FT(CGAL::to_double(vn.z()) / len)));
        }
    }

    // 2. Edge bisectors
    for (const auto& [edge, faces] : edge_to_faces) {
        if (faces.size() >= 2) {
            EK::Vector_3 en = face_normals[faces[0]] + face_normals[faces[1]];
            double len = std::sqrt(CGAL::to_double(en.squared_length()));
            if (len > 1e-6) {
                candidate_dirs.push_back(EK::Vector_3(FT(CGAL::to_double(en.x()) / len), FT(CGAL::to_double(en.y()) / len), FT(CGAL::to_double(en.z()) / len)));
            }
        }
    }

    // 3. Face normals (normalized)
    for (const auto& fn : face_normals) {
        double len = std::sqrt(CGAL::to_double(fn.squared_length()));
        if (len > 1e-6) {
            candidate_dirs.push_back(EK::Vector_3(FT(CGAL::to_double(fn.x()) / len), FT(CGAL::to_double(fn.y()) / len), FT(CGAL::to_double(fn.z()) / len)));
        }
    }

    EK::Vector_3 best_dir(FT(1), FT(0), FT(0));
    int best_loop_count = 999999;
    FT best_patch_score = -1;
    std::vector<ExactMesh::Face_index> best_patch_faces;

    struct CandidateLog {
        EK::Vector_3 dir;
        FT score;
        size_t face_count;
        int cycle_count;
    };
    std::vector<CandidateLog> ranked_candidates;
    CandidateLog best_downward{EK::Vector_3(0, 0, 0), FT(-1), 0, -1};
    CandidateLog best_upward{EK::Vector_3(0, 0, 0), FT(-1), 0, -1};

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
                auto f = ExactMesh::Face_index(f_idx);
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
        FT current_patch_score = 0;
        for (const auto& [root, comp_faces] : components) {
            FT comp_score = 0;
            for (auto f : comp_faces) {
                size_t f_idx = (size_t)f;
                FT a = face_areas[f_idx];
                FT dot = face_normals[f_idx] * d;
                if (dot > min_dot) {
                    const auto& c = face_centroids[f_idx];
                    FT depth = FT(r_bound) - (c.x()*d.x() + c.y()*d.y() + c.z()*d.z());
                    comp_score += a * (dot - min_dot) * depth;
                }
            }
            if (comp_score > current_patch_score) {
                current_patch_score = comp_score;
                largest_comp = comp_faces;
            }
        }

        std::vector<ExactMesh::Halfedge_index> border_halfedges;
        CGAL::Polygon_mesh_processing::border_halfedges(
            largest_comp, mesh_part, std::back_inserter(border_halfedges)
        );

        std::map<int, int> next_v;
        for (auto h : border_halfedges) {
            int u = (int)mesh_part.source(h);
            int v = (int)mesh_part.target(h);
            next_v[u] = v;
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
            ranked_candidates.push_back({d, current_patch_score, largest_comp.size(), 1});
            if (best_loop_count > 1 || current_patch_score > best_patch_score) {
                best_loop_count = 1;
                best_patch_score = current_patch_score;
                best_dir = d;
                best_patch_faces = largest_comp;
            }
        } else if (best_loop_count > 1 && cycle_count > 0 && (best_patch_score < 0 || current_patch_score > best_patch_score)) {
            best_loop_count = cycle_count;
            best_patch_score = current_patch_score;
            best_dir = d;
            best_patch_faces = largest_comp;
        }

        double dz_val = CGAL::to_double(d.z());
        if (dz_val < -0.85 && current_patch_score > best_downward.score) {
            best_downward = {d, current_patch_score, largest_comp.size(), cycle_count};
        }
        if (dz_val > 0.85 && current_patch_score > best_upward.score) {
            best_upward = {d, current_patch_score, largest_comp.size(), cycle_count};
        }
    }

    auto t_opt_end = std::chrono::steady_clock::now();
    double opt_ms = std::chrono::duration<double, std::milli>(t_opt_end - t_opt_start).count();
    std::cout << " Done in " << opt_ms << "ms." << std::endl;

    std::sort(ranked_candidates.begin(), ranked_candidates.end(), [](const auto& a, const auto& b) {
        return a.score > b.score;
    });

    std::cout << "      Top Candidates (cycles=1):" << std::endl;
    for (size_t i = 0; i < (std::min)(size_t(5), ranked_candidates.size()); ++i) {
        const auto& c = ranked_candidates[i];
        std::cout << "        #" << (i + 1) << ": dir=("
                  << CGAL::to_double(c.dir.x()) << ", "
                  << CGAL::to_double(c.dir.y()) << ", "
                  << CGAL::to_double(c.dir.z()) << ") score=" << CGAL::to_double(c.score)
                  << " faces=" << c.face_count << std::endl;
    }
    if (best_downward.face_count > 0) {
        std::cout << "      Best Downward (-Z hemisphere): dir=("
                  << CGAL::to_double(best_downward.dir.x()) << ", "
                  << CGAL::to_double(best_downward.dir.y()) << ", "
                  << CGAL::to_double(best_downward.dir.z()) << ") score=" << CGAL::to_double(best_downward.score)
                  << " faces=" << best_downward.face_count << " (cycles=" << best_downward.cycle_count << ")" << std::endl;
    }
    if (best_upward.face_count > 0) {
        std::cout << "      Best Upward (+Z hemisphere): dir=("
                  << CGAL::to_double(best_upward.dir.x()) << ", "
                  << CGAL::to_double(best_upward.dir.y()) << ", "
                  << CGAL::to_double(best_upward.dir.z()) << ") score=" << CGAL::to_double(best_upward.score)
                  << " faces=" << best_upward.face_count << " (cycles=" << best_upward.cycle_count << ")" << std::endl;
    }

    std::cout << "      Selected Best Dir: (" 
              << CGAL::to_double(best_dir.x()) << ", " << CGAL::to_double(best_dir.y()) << ", " << CGAL::to_double(best_dir.z())
              << ") with " << best_patch_faces.size() << " seed faces (score=" << CGAL::to_double(best_patch_score) << ")." << std::endl << std::flush;

    if (best_loop_count > 1) {
        throw std::runtime_error("Demoldability Error: Geometry contains internal undercut islands along all tested draw vectors (requires multi-stage side lifters).");
    }

    // Compute exact Upper Envelope mesh along best_dir within the OBB corridor of best_patch_faces
    auto env_res = compute_exact_upper_envelope_mesh(
        mesh_part, face_descriptors, face_normals, is_handled, best_dir, best_patch_faces
    );

    if (env_res.solid_wedge.number_of_faces() == 0) {
        return {best_dir, {}, {}, 0};
    }

    // Unite envelope diagram faces with seed patch faces (including negative-draft facets within tolerance)
    std::set<size_t> all_handled_faces = env_res.source_faces;
    for (auto f : best_patch_faces) {
        all_handled_faces.insert((size_t)f);
    }

    return {best_dir, env_res.solid_wedge, all_handled_faces, 1};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
