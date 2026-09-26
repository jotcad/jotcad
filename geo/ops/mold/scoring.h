#pragma once

#include "types.h"
#include <vector>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Represents the physical evaluation of a candidate draw direction.
 * 
 * Scores a candidate pull direction purely by the physical 3D surface area (in mm^2)
 * of unhandled model faces that the direction takes responsibility for molding.
 */
struct CandidateScore {
    FT net_score = FT(0);            // Net score: responsible_area - encroachment_penalty
    FT responsible_area = FT(0);      // Surface area in mm^2 of unhandled faces molded
    FT draft_weighted_area = FT(0);  // Tie-breaker: draft alignment (sum of area * dot)

    bool operator>(const CandidateScore& other) const {
        if (net_score != other.net_score) {
            return net_score > other.net_score;
        }
        if (responsible_area != other.responsible_area) {
            return responsible_area > other.responsible_area;
        }
        return draft_weighted_area > other.draft_weighted_area;
    }

    bool operator<(const CandidateScore& other) const {
        if (net_score != other.net_score) {
            return net_score < other.net_score;
        }
        if (responsible_area != other.responsible_area) {
            return responsible_area < other.responsible_area;
        }
        return draft_weighted_area < other.draft_weighted_area;
    }
};

/**
 * @brief Computes the CandidateScore for a given draw direction d.
 * 
 * Iterates through all unhandled faces of the part mesh and accumulates the
 * true 3D surface area of all faces satisfying normal draft (n_f . d >= min_dot).
 * If prior pieces exist, penalizes candidate directions that encroach into
 * already-handled geometry (n_handled . d > 0) using PRIOR_ENCROACHMENT_PENALTY_WEIGHT.
 */
inline CandidateScore score_candidate_direction(
    const EK::Vector_3& d,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    boolean::ExactMesh::Property_map<boolean::ExactMesh::Face_index, bool> is_handled,
    const FT& min_dot = FT(0)
) {
    CandidateScore score;
    bool has_prior_handled = false;
    FT encroachment_penalty = FT(0);
    const FT w_encroach = FT(optimizer_constants::PRIOR_ENCROACHMENT_PENALTY_WEIGHT);

    for (auto f : face_descriptors) {
        size_t idx = f.idx();
        FT dot = face_normals[idx] * d;
        if (!is_handled[f]) {
            if (dot >= min_dot) {
                FT a = face_areas[idx];
                score.responsible_area += a;
                score.draft_weighted_area += a * dot;
            }
        } else {
            has_prior_handled = true;
            if (dot > FT(0)) {
                encroachment_penalty += w_encroach * face_areas[idx] * dot;
            }
        }
    }

    score.net_score = has_prior_handled ? (score.responsible_area - encroachment_penalty) : score.responsible_area;
    return score;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
