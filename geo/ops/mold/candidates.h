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

    // 1. Dead-zone eliminating target directions:
    // - Opposing pull vector of prior pieces in the chain (-d_prior) to eliminate parting wedges
    // - Uncovered stock boundary face normals to eliminate untouched stock regions
    auto dead_zone_targets = get_dead_zone_targets(prior_draw_dirs);
    for (const auto& target : dead_zone_targets) {
        raw_candidates.push_back(target);
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
                            raw_candidates.push_back(snapped);
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
