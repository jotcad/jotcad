#pragma once

#include "types.h"
#include "modes.h"
#include <vector>
#include <cmath>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Evaluates the unconstrained draft objective score and normal gradient for a direction d.
 */
inline std::pair<double, Vector3d> evaluate_draft_objective_and_gradient(
    const Vector3d& d,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    FaceBoolMap is_handled,
    double min_dot
) {
    double score = 0.0;
    Vector3d grad(0.0, 0.0, 0.0);

    for (auto f : face_descriptors) {
        if (!is_handled[f]) {
            size_t idx = f.idx();
            Vector3d n(face_normals[idx]);
            double dot = n.dot(d);
            if (dot >= min_dot) {
                double a = CGAL::to_double(face_areas[idx]);
                score += a * (dot - min_dot);
                grad = grad + (n * a);
            }
        }
    }

    return {score, grad};
}

/**
 * @brief Continuous spherical gradient ascent on the 2D manifold S2.
 *
 * Climbs from an initial seed direction to the exact continuous local summit of draft
 * and visibility on the unit sphere in 5-8 steps with backtracking line search.
 */
inline Vector3d climb_spherical_hill(
    const Vector3d& initial_d,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    FaceBoolMap is_handled,
    double min_dot,
    int max_steps = 10
) {
    Vector3d curr_d = initial_d.normalized();
    auto [curr_score, curr_grad] = evaluate_draft_objective_and_gradient(
        curr_d, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );

    for (int step = 0; step < max_steps; ++step) {
        // Project unconstrained gradient onto the tangent plane of S2 at curr_d
        double g_dot_d = curr_grad.dot(curr_d);
        Vector3d tangent_grad = curr_grad - (curr_d * g_dot_d);
        double tg_norm = tangent_grad.norm();

        if (tg_norm < 1e-6) {
            // Reached stationary point / local summit on S2
            break;
        }

        Vector3d search_dir = tangent_grad * (1.0 / tg_norm);

        // Backtracking line search along the spherical geodesic
        double step_size = 0.1; // ~5.7 degrees initial step
        bool accepted = false;

        for (int ls = 0; ls < 5; ++ls) {
            Vector3d candidate_d = (curr_d + (search_dir * step_size)).normalized();
            auto [cand_score, cand_grad] = evaluate_draft_objective_and_gradient(
                candidate_d, face_descriptors, face_normals, face_areas, is_handled, min_dot
            );

            if (cand_score > curr_score + 1e-9) {
                curr_d = candidate_d;
                curr_score = cand_score;
                curr_grad = cand_grad;
                accepted = true;
                break;
            }

            step_size *= 0.5;
        }

        if (!accepted) {
            // Reached local maximum within angular precision
            break;
        }
    }

    return curr_d;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
