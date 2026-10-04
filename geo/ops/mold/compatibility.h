#pragma once

#include "types.h"
#include <vector>
#include <map>
#include <set>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Checks if candidate_faces and vector d are compatible with previous pieces in the chain.
 * 
 * Physical viability, parting clearance, and backdraft freedom are authoritatively
 * verified via CSG difference and verify_piece_demoldability.
 */
inline bool is_candidate_compatible_with_chain(
    const std::vector<ExactMesh::Face_index>& candidate_faces,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceBoolMap is_handled,
    const EK::Vector_3& d,
    const std::vector<EK::Vector_3>& prior_draw_dirs
) {
    if (candidate_faces.empty()) {
        return false;
    }

    FT d_len_sq = d.squared_length();
    if (d_len_sq == FT(0)) {
        return false;
    }

    // Kinematic Independence Mandate:
    // A demolding sequence cannot pull in the same direction or within an acute cone (< 25 degrees)
    // of an earlier piece in the same chain (d . p > 0 and (d . p)^2 / (|d|^2 |p|^2) >= 0.8).
    for (const auto& p : prior_draw_dirs) {
        FT p_len_sq = p.squared_length();
        if (p_len_sq == FT(0)) continue;

        FT dot = d * p;
        if (dot > FT(0)) {
            FT lhs = dot * dot;
            FT rhs = (FT(8) / FT(10)) * d_len_sq * p_len_sq;
            if (lhs >= rhs) {
                return false; // Incompatible: near-parallel to an existing draw direction in chain
            }
        }
    }

    return true;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
