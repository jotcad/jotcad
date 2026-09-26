#pragma once

#include "types.h"
#include <vector>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Identifies uncovered stock boundary directions (cardinal face normals)
 * that have not been reached by any prior piece in the chain.
 */
inline std::vector<EK::Vector_3> get_uncovered_stock_directions(
    const std::vector<EK::Vector_3>& prior_draw_dirs
) {
    const std::vector<EK::Vector_3> stock_normals = {
        EK::Vector_3(FT(1), FT(0), FT(0)),
        EK::Vector_3(FT(-1), FT(0), FT(0)),
        EK::Vector_3(FT(0), FT(1), FT(0)),
        EK::Vector_3(FT(0), FT(-1), FT(0)),
        EK::Vector_3(FT(0), FT(0), FT(1)),
        EK::Vector_3(FT(0), FT(0), FT(-1))
    };

    if (prior_draw_dirs.empty()) {
        return stock_normals;
    }

    std::vector<EK::Vector_3> uncovered;
    for (const auto& sn : stock_normals) {
        bool reached = false;
        for (const auto& pd : prior_draw_dirs) {
            if (sn * pd > FT(0)) {
                reached = true;
                break;
            }
        }
        if (!reached) {
            uncovered.push_back(sn);
        }
    }
    return uncovered;
}

/**
 * @brief Gathers all dead-zone eliminating target directions for the current search depth:
 * 1. Antipodal complement vectors (-d_prior) to prevent parting wedges.
 * 2. Uncovered stock boundary face normals to prevent abandoned stock regions.
 */
inline std::vector<EK::Vector_3> get_dead_zone_targets(
    const std::vector<EK::Vector_3>& prior_draw_dirs
) {
    std::vector<EK::Vector_3> targets;
    for (const auto& pd : prior_draw_dirs) {
        targets.push_back(-pd);
    }
    auto uncovered_stock = get_uncovered_stock_directions(prior_draw_dirs);
    targets.insert(targets.end(), uncovered_stock.begin(), uncovered_stock.end());
    return targets;
}

/**
 * @brief Snaps a candidate draw direction d toward target direction u along their great-circle arc,
 * stopping at the boundary of the feasible draft cone defined by face_normals.
 * 
 * In pure EK::FT exact rational arithmetic:
 * v(t) = (1 - t)*d + t*u
 * Face constraint: n_f * v(t) >= 0
 * 
 * @return EK::Vector_3 The snapped direction on the cone boundary closest to u.
 */
inline EK::Vector_3 snap_vector_within_draft_cone(
    const EK::Vector_3& d,
    const EK::Vector_3& u,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<size_t>& face_indices
) {
    if (d == u || u.squared_length() == FT(0)) return d;

    // Check if u and d are antipodal (u = -d)
    EK::Vector_3 cp = CGAL::cross_product(d, u);
    if (cp.squared_length() == FT(0) && (d * u) < FT(0)) {
        bool u_feasible = true;
        for (size_t f_idx : face_indices) {
            if (face_normals[f_idx] * u < FT(0)) {
                u_feasible = false;
                break;
            }
        }
        return u_feasible ? u : d;
    }

    // Determine maximum safe rotation parameter t* in [0, 1]
    // v(t) = (1 - t)*d + t*u
    // n_f * v(t) = (1 - t)*(n_f * d) + t*(n_f * u) >= 0
    FT t_max = FT(1);

    for (size_t f_idx : face_indices) {
        FT a_f = face_normals[f_idx] * d;
        FT b_f = face_normals[f_idx] * u;

        if (b_f < FT(0)) {
            FT denom = a_f - b_f;
            if (denom > FT(0)) {
                FT t_f = a_f / denom;
                if (t_f < t_max) {
                    t_max = t_f;
                }
            } else {
                t_max = FT(0);
                break;
            }
        }
    }

    if (t_max <= FT(0)) {
        return d;
    }

    EK::Vector_3 snapped = (FT(1) - t_max) * d + t_max * u;
    if (snapped.squared_length() == FT(0)) return d;
    return snapped;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
