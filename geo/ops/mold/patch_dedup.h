#pragma once

#include <vector>
#include <algorithm>
#include <iostream>
#include "types.h"
#include "patch.h"

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Candidate draw direction with its extracted cavity patch and ranking metrics.
 */
struct ScoredCandidate {
    EK::Vector_3 dir;
    CandidatePatch patch;
    FT virgin_area;
    FT total_patch_area;
    FT min_dot_margin = FT(0); // Worst-case draft margin: min_{f in patch} (n_f . dir)
};

/**
 * @brief Computes the Jaccard similarity metric between two candidate patches on virgin cavity faces:
 *        J(P_A, P_B) = Area(P_A \cap P_B) / Area(P_A \cup P_B)
 * 
 * Evaluated in pure EK::FT exact rational arithmetic with zero floating-point conversions.
 */
inline FT compute_patch_jaccard(
    const std::vector<size_t>& virgin_faces_a,
    const std::vector<size_t>& virgin_faces_b,
    const std::vector<FT>& face_areas,
    const FT& virgin_area_a,
    const FT& virgin_area_b
) {
    if (virgin_faces_a.empty() || virgin_faces_b.empty()) return FT(0);

    // Compute intersection area via two-pointer scan over sorted face indices
    FT intersection_area = FT(0);
    size_t i = 0, j = 0;
    while (i < virgin_faces_a.size() && j < virgin_faces_b.size()) {
        if (virgin_faces_a[i] == virgin_faces_b[j]) {
            intersection_area += face_areas[virgin_faces_a[i]];
            i++;
            j++;
        } else if (virgin_faces_a[i] < virgin_faces_b[j]) {
            i++;
        } else {
            j++;
        }
    }

    FT union_area = virgin_area_a + virgin_area_b - intersection_area;
    if (union_area <= FT(0)) return FT(0);
    return intersection_area / union_area;
}

/**
 * @brief Deduplicates candidate patches based on Jaccard virgin-face similarity.
 *        Merges candidates with J >= jaccard_threshold (default 0.85), retaining
 *        the candidate with maximum virgin area (tie-breaking on worst-case margin).
 * 
 * Collapses hundreds of redundant candidates differing by only a few degrees into
 * a small set of distinct physical cavity feature champions (reducing fanout from ~1800 to ~4-6).
 */
inline std::vector<ScoredCandidate> deduplicate_candidate_patches(
    std::vector<ScoredCandidate>& candidates,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const std::vector<bool>& is_handled,
    const FT& jaccard_threshold = FT(85) / FT(100)
) {
    if (candidates.size() <= 1) return candidates;

    struct CandidateData {
        ScoredCandidate cand;
        std::vector<size_t> virgin_faces;
    };

    std::vector<CandidateData> cand_data;
    cand_data.reserve(candidates.size());

    for (auto& sc : candidates) {
        CandidateData cd;
        cd.cand = std::move(sc);

        // Precompute sorted virgin face index vector and worst-case draft margin
        cd.virgin_faces.reserve(cd.cand.patch.faces.size());
        FT min_dot = FT(1e9);
        for (auto f : cd.cand.patch.faces) {
            size_t f_idx = (size_t)f.idx();
            if (!is_handled.empty() && f_idx < is_handled.size() && !is_handled[f_idx]) {
                cd.virgin_faces.push_back(f_idx);
            }
            FT dot = face_normals[f_idx] * cd.cand.dir;
            if (dot < min_dot) min_dot = dot;
        }
        std::sort(cd.virgin_faces.begin(), cd.virgin_faces.end());
        cd.cand.min_dot_margin = (min_dot < FT(1e9)) ? min_dot : FT(0);
        cand_data.push_back(std::move(cd));
    }

    // Greedy clustering by virgin face Jaccard similarity:
    // Champions retain the candidate with highest virgin area (tie-break on margin).
    std::vector<CandidateData> champions;

    for (auto& item : cand_data) {
        bool merged = false;
        for (auto& champ : champions) {
            FT jaccard = compute_patch_jaccard(
                item.virgin_faces, champ.virgin_faces, face_areas,
                item.cand.virgin_area, champ.cand.virgin_area
            );
            if (jaccard >= jaccard_threshold) {
                // If the new item captures strictly more virgin area, it replaces champ.
                // If virgin area is tied, tie-break on draft clearance margin:
                if (item.cand.virgin_area > champ.cand.virgin_area ||
                   (item.cand.virgin_area == champ.cand.virgin_area && item.cand.min_dot_margin > champ.cand.min_dot_margin)) {
                    champ = std::move(item);
                }
                merged = true;
                break;
            }
        }
        if (!merged) {
            champions.push_back(std::move(item));
        }
    }

    std::cout << "      ↳ [Dedup] Collapsed " << candidates.size() << " candidates -> "
              << champions.size() << " unique feature patches (threshold="
              << CGAL::to_double(jaccard_threshold) << ")." << std::endl;

    std::vector<ScoredCandidate> result;
    result.reserve(champions.size());
    for (auto& ch : champions) {
        result.push_back(std::move(ch.cand));
    }
    return result;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
