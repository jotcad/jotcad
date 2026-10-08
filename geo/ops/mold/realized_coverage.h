#pragma once

#include "types.h"
#include "policy.h"
#include "envelope.h"
#include "patch_dedup.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <vector>
#include <future>
#include <thread>
#include <mutex>

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
    std::mutex* envelope_cache_mutex = nullptr,
    bool* out_was_cached = nullptr
) {
    VectorKey vkey{d.x(), d.y(), d.z()};
    if (envelope_cache_mutex) {
        std::lock_guard<std::mutex> lock(*envelope_cache_mutex);
        auto it = envelope_cache.find(vkey);
        if (out_was_cached) *out_was_cached = (it != envelope_cache.end());
        if (it == envelope_cache.end()) {
            auto env_computed = compute_exact_upper_envelope_mesh(
                mesh_part, face_descriptors, face_normals, d, padding, stock_box_mesh, /*build_wedge=*/false
            );
            it = envelope_cache.emplace(vkey, std::move(env_computed)).first;
        }
        return it->second;
    } else {
        auto it = envelope_cache.find(vkey);
        if (out_was_cached) *out_was_cached = (it != envelope_cache.end());
        if (it == envelope_cache.end()) {
            auto env_computed = compute_exact_upper_envelope_mesh(
                mesh_part, face_descriptors, face_normals, d, padding, stock_box_mesh, /*build_wedge=*/false
            );
            it = envelope_cache.emplace(vkey, std::move(env_computed)).first;
        }
        return it->second;
    }
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
    std::mutex* envelope_cache_mutex = nullptr,
    const MoldDecompositionPolicy& policy = MoldDecompositionPolicy()
) {
    auto t0 = std::chrono::steady_clock::now();
    const size_t input_count = champions.size();

    // 1. Identify which candidate directions are missing from envelope_cache
    std::vector<EK::Vector_3> unique_missing_dirs;
    std::set<VectorKey> seen_missing;
    size_t cache_hits = 0;

    {
        std::unique_lock<std::mutex> lock;
        if (envelope_cache_mutex) lock = std::unique_lock<std::mutex>(*envelope_cache_mutex);

        for (const auto& sc : champions) {
            VectorKey vkey{sc.dir.x(), sc.dir.y(), sc.dir.z()};
            if (envelope_cache.find(vkey) != envelope_cache.end()) {
                cache_hits++;
            } else if (seen_missing.insert(vkey).second) {
                unique_missing_dirs.push_back(sc.dir);
            }
        }
    }
    const size_t computed = unique_missing_dirs.size();

    // 2. Parallel envelope computation for all missing directions across hardware threads
    if (!unique_missing_dirs.empty()) {
        std::vector<EnvelopeMeshResult> computed_envs(unique_missing_dirs.size());
        unsigned int hw_threads = std::thread::hardware_concurrency();
        unsigned int max_workers = (envelope_cache_mutex != nullptr) ? 2u : hw_threads;
        unsigned int num_workers = std::max(1u, std::min(max_workers, (unsigned int)unique_missing_dirs.size()));

        std::vector<std::future<void>> futures;
        futures.reserve(num_workers);

        for (unsigned int w = 0; w < num_workers; ++w) {
            size_t start = (w * unique_missing_dirs.size()) / num_workers;
            size_t end = ((w + 1) * unique_missing_dirs.size()) / num_workers;
            if (start >= end) continue;
            futures.push_back(std::async(std::launch::async, [&, start, end]() {
                for (size_t idx = start; idx < end; ++idx) {
                    computed_envs[idx] = compute_exact_upper_envelope_mesh(
                        mesh_part, face_descriptors, face_normals,
                        unique_missing_dirs[idx], padding, stock_box_mesh, /*build_wedge=*/false
                    );
                }
            }));
        }
        for (auto& fut : futures) {
            fut.get();
        }

        // Store into memoized cache sequentially under lock
        {
            std::unique_lock<std::mutex> lock;
            if (envelope_cache_mutex) lock = std::unique_lock<std::mutex>(*envelope_cache_mutex);

            for (size_t idx = 0; idx < unique_missing_dirs.size(); ++idx) {
                const auto& d = unique_missing_dirs[idx];
                VectorKey vkey{d.x(), d.y(), d.z()};
                if (envelope_cache.find(vkey) == envelope_cache.end()) {
                    envelope_cache.emplace(vkey, std::move(computed_envs[idx]));
                }
            }
        }
    }

    // 3. Populate realized candidates from populated envelope cache
    std::vector<ScoredCandidate> realized;
    realized.reserve(champions.size());

    for (auto& sc : champions) {
        VectorKey vkey{sc.dir.x(), sc.dir.y(), sc.dir.z()};
        std::set<size_t> candidate_handled;
        {
            std::unique_lock<std::mutex> lock;
            if (envelope_cache_mutex) lock = std::unique_lock<std::mutex>(*envelope_cache_mutex);

            auto it = envelope_cache.find(vkey);
            if (it == envelope_cache.end() || it->second.source_faces.empty()) continue;
            candidate_handled = it->second.source_faces;
        }
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
        realized = deduplicate_candidate_patches(realized, face_normals, face_areas, parent_is_handled, policy.jaccard_threshold);
    }
    return realized;
}

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
    const MoldDecompositionPolicy& policy
) {
    return realize_candidate_coverage(
        champions, mesh_part, face_descriptors, face_normals, face_areas,
        parent_is_handled, padding, stock_box_mesh, envelope_cache, nullptr, policy
    );
}

} // namespace mold
} // namespace geo
} // namespace jotcad

