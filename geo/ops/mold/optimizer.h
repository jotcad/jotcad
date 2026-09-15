#pragma once
#include "types.h"
#include "visibility.h"
#include "modes.h"
#include "climb.h"
#include "boundary.h"
#include <cmath>
#include <CGAL/Polygon_mesh_processing/border.h>

namespace jotcad {
namespace geo {
namespace mold {

// Pure EK::FT tangent basis orthogonal to d
inline std::pair<EK::Vector_3, EK::Vector_3> compute_exact_tangent_basis(const EK::Vector_3& d) {
    EK::FT dx = d.x();
    EK::FT dy = d.y();
    EK::FT dz = d.z();
    EK::FT abs_dx = (dx < EK::FT(0)) ? -dx : dx;
    EK::FT abs_dy = (dy < EK::FT(0)) ? -dy : dy;
    EK::FT abs_dz = (dz < EK::FT(0)) ? -dz : dz;

    EK::Vector_3 ref(EK::FT(0), EK::FT(0), EK::FT(0));
    if (abs_dx <= abs_dy && abs_dx <= abs_dz) {
        ref = EK::Vector_3(EK::FT(1), EK::FT(0), EK::FT(0));
    } else if (abs_dy <= abs_dz) {
        ref = EK::Vector_3(EK::FT(0), EK::FT(1), EK::FT(0));
    } else {
        ref = EK::Vector_3(EK::FT(0), EK::FT(0), EK::FT(1));
    }

    EK::Vector_3 u = CGAL::cross_product(d, ref);
    EK::Vector_3 v = CGAL::cross_product(d, u);
    return {u, v};
}

struct BoundingBox2D {
    EK::FT u_min = 0, u_max = 0;
    EK::FT v_min = 0, v_max = 0;

    bool overlaps(const BoundingBox2D& other) const {
        if (u_max < other.u_min || other.u_max < u_min) return false;
        if (v_max < other.v_min || other.v_max < v_min) return false;
        return true;
    }
};

inline int count_component_boundary_cycles(
    const std::vector<ExactMesh::Face_index>& comp_faces,
    const ExactMesh& mesh_part
) {
    std::vector<ExactMesh::Halfedge_index> border_halfedges;
    CGAL::Polygon_mesh_processing::border_halfedges(
        comp_faces, mesh_part, std::back_inserter(border_halfedges)
    );

    std::map<int, int> next_v;
    for (auto h : border_halfedges) {
        int u = (int)mesh_part.source(h);
        int v = (int)mesh_part.target(h);
        next_v[u] = v;
    }

    std::set<int> visited;
    int cycle_count = 0;
    for (auto h : border_halfedges) {
        int start = (int)mesh_part.source(h);
        if (visited.count(start)) continue;

        int curr = start;
        int step = 0;
        while (curr != -1 && !visited.count(curr)) {
            visited.insert(curr);
            auto it = next_v.find(curr);
            if (it == next_v.end()) break;
            int nxt = it->second;
            step++;
            curr = nxt;
            if (curr == start) break;
        }
        if (curr == start && step >= 3) {
            cycle_count++;
        }
    }
    return cycle_count;
}


struct PartingOptimizationResult {
    EK::Vector_3 best_dir;
    ExactMesh solid_wedge;
    std::set<size_t> source_faces;
    int cycle_count;
    std::vector<std::vector<Point_3>> boundary_loops_3d;
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

    // Phase 3: Area-Weighted Normal Mode Clustering + Continuous Spherical Hill Climbing
    auto mode_seeds = compute_normal_modes(face_descriptors, face_normals, face_areas, is_handled, 6);

    // Complement mode seeds with cardinal axes to guarantee full spatial coverage
    const std::vector<Vector3d> cardinal_dirs = {
        { 0,  0,  1}, { 0,  0, -1},
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0}
    };
    for (const auto& card : cardinal_dirs) {
        bool duplicate = false;
        for (const auto& s : mode_seeds) {
            if (card.dot(s) > 0.95) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) mode_seeds.push_back(card);
    }

    double min_dot_d = CGAL::to_double(min_dot);
    std::vector<Vector3d> optimized_summits;
    for (const auto& seed : mode_seeds) {
        Vector3d summit = climb_spherical_hill(
            seed, face_descriptors, face_normals, face_areas, is_handled, min_dot_d
        );
        bool duplicate = false;
        for (const auto& opt_s : optimized_summits) {
            if (summit.dot(opt_s) > 0.99) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            optimized_summits.push_back(summit);
            candidate_dirs.push_back(summit.to_exact());
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

        auto [u_basis, v_basis] = compute_exact_tangent_basis(d);

        struct CompData {
            std::vector<ExactMesh::Face_index> faces;
            FT score = 0;
            int cycle_count = 0;
            BoundingBox2D bbox;
        };

        std::vector<CompData> disk_comps;
        std::vector<CompData> non_disk_comps;

        for (const auto& [root, comp_faces] : components) {
            FT comp_score = 0;
            BoundingBox2D bbox;
            bool first_pt = true;

            for (auto f : comp_faces) {
                size_t f_idx = (size_t)f;
                FT a = face_areas[f_idx];
                FT dot = face_normals[f_idx] * d;
                if (dot > min_dot) {
                    const auto& c = face_centroids[f_idx];
                    FT depth = FT(r_bound) - (c.x()*d.x() + c.y()*d.y() + c.z()*d.z());
                    comp_score += a * (dot - min_dot) * depth;
                }

                auto h = mesh_part.halfedge(f);
                for (int i = 0; i < 3; ++i) {
                    const auto& p = mesh_part.point(mesh_part.target(h));
                    FT up = p.x()*u_basis.x() + p.y()*u_basis.y() + p.z()*u_basis.z();
                    FT vp = p.x()*v_basis.x() + p.y()*v_basis.y() + p.z()*v_basis.z();
                    if (first_pt) {
                        bbox.u_min = bbox.u_max = up;
                        bbox.v_min = bbox.v_max = vp;
                        first_pt = false;
                    } else {
                        if (up < bbox.u_min) bbox.u_min = up;
                        if (up > bbox.u_max) bbox.u_max = up;
                        if (vp < bbox.v_min) bbox.v_min = vp;
                        if (vp > bbox.v_max) bbox.v_max = vp;
                    }
                    h = mesh_part.next(h);
                }
            }

            if (comp_score <= FT(0)) continue;

            int cycles = count_component_boundary_cycles(comp_faces, mesh_part);
            CompData cd{comp_faces, comp_score, cycles, bbox};

            if (cycles == 1) {
                disk_comps.push_back(std::move(cd));
            } else if (cycles > 1) {
                non_disk_comps.push_back(std::move(cd));
            }
        }

        std::vector<ExactMesh::Face_index> candidate_patch_faces;
        FT current_patch_score = 0;
        int cycle_count = 0;

        if (!disk_comps.empty()) {
            std::sort(disk_comps.begin(), disk_comps.end(), [](const auto& a, const auto& b) {
                return a.score > b.score;
            });

            std::vector<BoundingBox2D> accepted_bboxes;
            candidate_patch_faces = disk_comps[0].faces;
            current_patch_score = disk_comps[0].score;
            accepted_bboxes.push_back(disk_comps[0].bbox);

            for (size_t i = 1; i < disk_comps.size(); ++i) {
                const auto& cand = disk_comps[i];
                bool overlaps = false;
                for (const auto& acc_box : accepted_bboxes) {
                    if (cand.bbox.overlaps(acc_box)) {
                        overlaps = true;
                        break;
                    }
                }
                if (!overlaps) {
                    candidate_patch_faces.insert(candidate_patch_faces.end(), cand.faces.begin(), cand.faces.end());
                    current_patch_score += cand.score;
                    accepted_bboxes.push_back(cand.bbox);
                }
            }
            cycle_count = 1;
        } else if (!non_disk_comps.empty()) {
            std::sort(non_disk_comps.begin(), non_disk_comps.end(), [](const auto& a, const auto& b) {
                return a.score > b.score;
            });
            candidate_patch_faces = non_disk_comps[0].faces;
            current_patch_score = non_disk_comps[0].score;
            cycle_count = non_disk_comps[0].cycle_count;
        } else {
            continue;
        }

        if (cycle_count == 1) {
            ranked_candidates.push_back({d, current_patch_score, candidate_patch_faces.size(), 1});
            if (best_loop_count > 1 || current_patch_score > best_patch_score) {
                best_loop_count = 1;
                best_patch_score = current_patch_score;
                best_dir = d;
                best_patch_faces = candidate_patch_faces;
            }
        } else if (best_loop_count > 1 && cycle_count > 0 && (best_patch_score < 0 || current_patch_score > best_patch_score)) {
            best_loop_count = cycle_count;
            best_patch_score = current_patch_score;
            best_dir = d;
            best_patch_faces = candidate_patch_faces;
        }

        double dz_val = CGAL::to_double(d.z());
        if (dz_val < -0.85 && current_patch_score > best_downward.score) {
            best_downward = {d, current_patch_score, candidate_patch_faces.size(), cycle_count};
        }
        if (dz_val > 0.85 && current_patch_score > best_upward.score) {
            best_upward = {d, current_patch_score, candidate_patch_faces.size(), cycle_count};
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

    // Audit selected patch projected boundary for simplicity before envelope extraction
    auto audit = audit_patch_projected_boundary_simplicity(mesh_part, best_patch_faces, best_dir, "Selected Patch Projected Boundary");

    // Compute exact Upper Envelope mesh along best_dir within the OBB corridor of best_patch_faces
    auto env_res = compute_exact_upper_envelope_mesh(
        mesh_part, face_descriptors, face_normals, is_handled, best_dir, best_patch_faces, params.padding
    );

    if (env_res.solid_wedge.number_of_faces() == 0) {
        return {best_dir, {}, {}, 0, env_res.boundary_loops_3d};
    }

    // Unite envelope diagram faces with seed patch faces (including negative-draft facets within tolerance)
    std::set<size_t> all_handled_faces = env_res.source_faces;
    for (auto f : best_patch_faces) {
        all_handled_faces.insert((size_t)f);
    }

    return {best_dir, env_res.solid_wedge, all_handled_faces, 1, env_res.boundary_loops_3d};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
