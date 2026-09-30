#pragma once

#include "types.h"
#include "snap.h"
#include <vector>
#include <cmath>
#include <random>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Generates candidate mold draw directions on S^2.
 * 
 * Combines dead-zone eliminating target directions (antipodal prior directions
 * and uncovered stock boundaries), exploratory random sampling on S^2, and
 * cone-snapped directions that rotate exploratory vectors toward dead-zone targets
 * within their feasible draft cones.
 */
inline std::vector<EK::Vector_3> generate_candidate_directions(
    const boolean::ExactMesh& mesh_part,
    const std::vector<boolean::ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    boolean::ExactMesh::Property_map<boolean::ExactMesh::Face_index, bool> is_handled,
    const std::vector<EK::Vector_3>& prior_draw_dirs = {},
    size_t num_exploratory = 16
) {
    std::vector<EK::Vector_3> raw_candidates;

    // 1. Dead-zone eliminating target directions (Section 4.2.3 #1 & #4):
    // - Opposing pull vector of prior pieces in the chain (-d_prior) to eliminate parting wedges
    // - Uncovered stock boundary face normals to eliminate untouched stock regions
    auto dead_zone_targets = get_dead_zone_targets(prior_draw_dirs);
    for (const auto& target : dead_zone_targets) {
        raw_candidates.push_back(target);
    }

    // 2. Active Wall Cross Products & Corner Bisectors across Mesh Edges (Section 4.2.3 #2 & #3):
    // For adjacent non-coplanar face pairs sharing an edge in mesh_part:
    // - Cross product n1 x n2 defines exact zero-draft sliding axes along intersecting walls
    // - Sum n1 + n2 defines exact +45° opening bisector away from both walls
    std::set<std::pair<size_t, size_t>> seen_adjacent_pairs;
    for (auto e : mesh_part.edges()) {
        auto h1 = mesh_part.halfedge(e, 0);
        auto h2 = mesh_part.halfedge(e, 1);
        if (h1 == boolean::ExactMesh::null_halfedge() || h2 == boolean::ExactMesh::null_halfedge()) continue;

        auto f1 = mesh_part.face(h1);
        auto f2 = mesh_part.face(h2);
        if (f1 == boolean::ExactMesh::null_face() || f2 == boolean::ExactMesh::null_face()) continue;

        size_t idx1 = (size_t)f1.idx();
        size_t idx2 = (size_t)f2.idx();
        if (is_handled[f1] && is_handled[f2]) continue;

        if (idx1 > idx2) std::swap(idx1, idx2);
        if (seen_adjacent_pairs.insert({idx1, idx2}).second) {
            const auto& n1 = face_normals[idx1];
            const auto& n2 = face_normals[idx2];
            FT n1_len_sq = n1.squared_length();
            FT n2_len_sq = n2.squared_length();
            if (n1_len_sq == FT(0) || n2_len_sq == FT(0)) continue;

            // Zero-draft sliding axes along intersecting wall pairs
            EK::Vector_3 cp = CGAL::cross_product(n1, n2);
            FT cp_len_sq = cp.squared_length();

            // Filter out coplanar face pairs: sin^2(theta) >= 1e-4 (|theta| >= 0.57 degrees)
            if (cp_len_sq * FT(10000) >= n1_len_sq * n2_len_sq) {
                double len = std::sqrt(CGAL::to_double(cp_len_sq));
                if (len > 1e-9) {
                    EK::Vector_3 cp_unit(
                        FT(CGAL::to_double(cp.x()) / len),
                        FT(CGAL::to_double(cp.y()) / len),
                        FT(CGAL::to_double(cp.z()) / len)
                    );
                    raw_candidates.push_back(cp_unit);
                    raw_candidates.push_back(-cp_unit);
                }
            }

            // Concave corner opening bisector (normalized to unit length on S^2)
            double len1 = std::sqrt(CGAL::to_double(n1_len_sq));
            double len2 = std::sqrt(CGAL::to_double(n2_len_sq));
            if (len1 > 1e-9 && len2 > 1e-9) {
                EK::Vector_3 u1(FT(CGAL::to_double(n1.x()) / len1), FT(CGAL::to_double(n1.y()) / len1), FT(CGAL::to_double(n1.z()) / len1));
                EK::Vector_3 u2(FT(CGAL::to_double(n2.x()) / len2), FT(CGAL::to_double(n2.y()) / len2), FT(CGAL::to_double(n2.z()) / len2));
                EK::Vector_3 bisector = u1 + u2;
                double bis_len = std::sqrt(CGAL::to_double(bisector.squared_length()));
                if (bis_len > 1e-9) {
                    EK::Vector_3 bis_unit(
                        FT(CGAL::to_double(bisector.x()) / bis_len),
                        FT(CGAL::to_double(bisector.y()) / bis_len),
                        FT(CGAL::to_double(bisector.z()) / bis_len)
                    );
                    raw_candidates.push_back(bis_unit);
                }
            }
        }
    }

    // Identify indices of unhandled faces
    std::vector<size_t> unhandled_indices;
    for (auto f : face_descriptors) {
        if (!is_handled[f]) {
            unhandled_indices.push_back((size_t)f.idx());
        }
    }

    // 2. Uniform Random Sampling on S^2
    // Replaces Fibonacci sphere with unbiased spherical sampling
    if (num_exploratory > 0) {
        // Deterministic PRNG with fixed seed for test reproducibility
        std::mt19937_64 rng(42 + prior_draw_dirs.size());
        std::uniform_real_distribution<double> dist(0.0, 1.0);

        for (size_t i = 0; i < num_exploratory; ++i) {
            double z = dist(rng) * 2.0 - 1.0;
            double phi = dist(rng) * 2.0 * M_PI;
            double r = std::sqrt((std::max)(0.0, 1.0 - z * z));
            double x = r * std::cos(phi);
            double y = r * std::sin(phi);
            auto d = EK::Vector_3(FT(x), FT(y), FT(z));
            raw_candidates.push_back(d);

            // 3. Dead-zone Cone Snapping:
            // For each unhandled face subset visible to d, snap d toward any
            // uncovered dead-zone target direction that d does not currently cover
            std::vector<size_t> visible_unhandled;
            for (size_t f_idx : unhandled_indices) {
                if (face_normals[f_idx] * d >= FT(0)) {
                    visible_unhandled.push_back(f_idx);
                }
            }

            if (!visible_unhandled.empty()) {
                for (const auto& target : dead_zone_targets) {
                    if (d * target <= FT(0)) {
                        EK::Vector_3 snapped = snap_vector_within_draft_cone(
                            d, target, face_normals, visible_unhandled
                        );
                        if (!(snapped == d) && snapped.squared_length() > FT(0)) {
                            double s_len = std::sqrt(CGAL::to_double(snapped.squared_length()));
                            if (s_len > 1e-9) {
                                raw_candidates.push_back(EK::Vector_3(
                                    FT(CGAL::to_double(snapped.x()) / s_len),
                                    FT(CGAL::to_double(snapped.y()) / s_len),
                                    FT(CGAL::to_double(snapped.z()) / s_len)
                                ));
                            }
                        }
                    }
                }
            }
        }
    }

    // 4. Exact Deduplication
    std::vector<EK::Vector_3> unique_candidates;
    for (const auto& d : raw_candidates) {
        FT len_sq_d = d.squared_length();
        if (len_sq_d == FT(0)) continue;

        bool duplicate = false;
        for (const auto& u : unique_candidates) {
            FT dot = d * u;
            if (dot > FT(0)) {
                EK::Vector_3 cp = CGAL::cross_product(d, u);
                if (cp.squared_length() == FT(0)) {
                    duplicate = true;
                    break;
                }
            }
        }
        if (!duplicate) {
            unique_candidates.push_back(d);
        }
    }

    return unique_candidates;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
