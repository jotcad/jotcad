#pragma once

#include "types.h"
#include "policy.h"
#include "envelope.h"
#include "patch_dedup.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <vector>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Exact rational key for memoizing per-direction upper envelope results.
 */
struct VectorKey {
    FT x, y, z;
    bool operator<(const VectorKey& other) const {
        if (x != other.x) return x < other.x;
        if (y != other.y) return y < other.y;
        return z < other.z;
    }
};

typedef std::map<VectorKey, EnvelopeMeshResult> EnvelopeCache;

/**
 * @brief Returns the memoized upper envelope for direction d, computing it on first use.
 *
 * The envelope depends only on (mesh_part, d, padding, stock): every forward-facing model face
 * participates as an occluder regardless of search state, so one entry serves every search node.
 */
inline const EnvelopeMeshResult& get_cached_envelope(
    const EK::Vector_3& d,
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const FT& padding,
    const ExactMesh* stock_box_mesh,
    EnvelopeCache& envelope_cache,
    bool* out_was_cached = nullptr
) {
    VectorKey vkey{d.x(), d.y(), d.z()};
    auto it = envelope_cache.find(vkey);
    if (out_was_cached) *out_was_cached = (it != envelope_cache.end());
    if (it == envelope_cache.end()) {
        auto env_computed = compute_exact_upper_envelope_mesh(
            mesh_part, face_descriptors, face_normals, d, padding, stock_box_mesh
        );
        it = envelope_cache.emplace(vkey, std::move(env_computed)).first;
    }
    return it->second;
}

/**
 * @brief Replaces each champion's normal-only predicted patch with its realized coverage:
 *        the model faces visible along its direction according to CGAL::upper_envelope_3.
 *
 * Virgin and total areas are recomputed from those faces in exact EK::FT. Candidates whose
 * envelope sees no unhandled area are dropped. Survivors are re-deduplicated, since distinct
 * predicted patches can realize to the same visible face set.
 */
inline std::vector<ScoredCandidate> realize_candidate_coverage(
    std::vector<ScoredCandidate>& champions,
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const std::vector<bool>& parent_is_handled,
    const FT& padding,
    const ExactMesh* stock_box_mesh,
    EnvelopeCache& envelope_cache,
    const MoldDecompositionPolicy& policy = MoldDecompositionPolicy()
) {
    auto t0 = std::chrono::steady_clock::now();
    size_t cache_hits = 0, computed = 0;
    const size_t input_count = champions.size();

    std::vector<ScoredCandidate> realized;
    realized.reserve(champions.size());

    for (auto& sc : champions) {
        bool was_cached = false;
        const auto& env = get_cached_envelope(
            sc.dir, mesh_part, face_descriptors, face_normals,
            padding, stock_box_mesh, envelope_cache, &was_cached
        );
        if (was_cached) cache_hits++; else computed++;
        if (env.source_faces.empty()) continue;

        std::set<size_t> candidate_handled = env.source_faces;
        if (policy.vertical_walls == VerticalWallPolicy::ALL_IN_PATCH) {
            for (auto f : sc.patch.faces) {
                size_t f_idx = (size_t)f.idx();
                if (face_normals[f_idx] * sc.dir == FT(0)) {
                    candidate_handled.insert(f_idx);
                }
            }
        }

        std::vector<ExactMesh::Face_index> faces;
        faces.reserve(candidate_handled.size());
        FT virgin_area = FT(0);
        FT total_area = FT(0);
        for (size_t f_idx : candidate_handled) {
            faces.push_back(ExactMesh::Face_index(f_idx));
            FT weight = FT(1);
            if (face_normals[f_idx] * sc.dir == FT(0)) {
                weight = policy.vertical_wall_weight;
            }
            FT a = face_areas[f_idx] * weight;
            total_area += a;
            if (!parent_is_handled[f_idx]) virgin_area += a;
        }

        // Forward Progress Mandate on realized (envelope-visible) coverage only.
        if (virgin_area <= FT(1) / FT(1000)) continue;

        sc.patch.faces = std::move(faces);
        sc.patch.total_area = total_area;
        sc.virgin_area = virgin_area;
        sc.total_patch_area = total_area;
        realized.push_back(std::move(sc));
    }

    std::sort(realized.begin(), realized.end(), [](const auto& a, const auto& b) {
        return a.virgin_area > b.virgin_area;
    });

    auto t1 = std::chrono::steady_clock::now();
    std::cout << "      ↳ [Realize] " << input_count << " champions -> " << realized.size()
              << " with envelope-visible virgin area (envelopes: " << computed << " computed, "
              << cache_hits << " cached, " << std::chrono::duration<double, std::milli>(t1 - t0).count()
              << " ms)." << std::endl << std::flush;

    if (realized.size() > 1) {
        realized = deduplicate_candidate_patches(realized, face_normals, face_areas, parent_is_handled);
    }
    return realized;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
