#pragma once

#include "kernel.h"
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {
namespace fix {

namespace detail {

// Helper: Canonicalize a polygon into its lexicographically minimal circular permutation.
// Returns the canonical index sequence and a boolean indicating if it was reversed.
template <typename Polygon>
inline std::pair<Polygon, bool> canonicalize_polygon(const Polygon& poly) {
    if (poly.empty()) return {poly, false};
    std::size_t n = poly.size();
    
    // Find min element
    auto min_it = std::min_element(poly.begin(), poly.end());
    std::size_t min_idx = std::distance(poly.begin(), min_it);

    // Compare forward and backward neighbor to determine canonical direction
    std::size_t prev_idx = (min_idx == 0) ? (n - 1) : (min_idx - 1);
    std::size_t next_idx = (min_idx + 1 == n) ? 0 : (min_idx + 1);

    bool reversed = (poly[prev_idx] < poly[next_idx]);

    Polygon canonical(n);
    if (!reversed) {
        for (std::size_t i = 0; i < n; ++i) {
            canonical[i] = poly[(min_idx + i) % n];
        }
    } else {
        for (std::size_t i = 0; i < n; ++i) {
            canonical[i] = poly[(min_idx + n - i) % n];
        }
    }
    return {canonical, reversed};
}

} // namespace detail

/**
 * @brief Regularizes a polygon soup for solid modeling by resolving hangnails and antiparallel duplicates.
 * 
 * In solid CAD models, coincident antiparallel face pairs ({[u,v,w], [u,w,v]}) with low-valency
 * boundary vertices represent zero-volume exterior fins (hangnails) or internal baffles.
 * Unlike naive single-face deduplication (which drops only one sheet and leaves a boundary hole),
 * this regularizer annihilates both sheets, restoring a closed 2-manifold edge.
 */
template <typename PointRange, typename PolygonRange>
inline std::size_t regularize_solid_soup_faces(const PointRange& points, PolygonRange& polygons) {
    if (polygons.empty()) return 0;

    std::size_t n_points = points.size();
    std::size_t initial_face_count = polygons.size();

    // 1. Compute vertex valency across all polygons
    std::vector<int> vertex_valency(n_points, 0);
    for (const auto& poly : polygons) {
        for (auto v : poly) {
            if (v < n_points) vertex_valency[v]++;
        }
    }

    // 2. Group polygons by canonical geometric boundary (sorted vertices)
    // Key: sorted vertex indices -> Value: list of (polygon_index, is_reversed)
    typedef typename PolygonRange::value_type Polygon;
    std::map<Polygon, std::vector<std::pair<std::size_t, bool>>> face_groups;

    for (std::size_t fi = 0; fi < polygons.size(); ++fi) {
        Polygon sorted_poly = polygons[fi];
        std::sort(sorted_poly.begin(), sorted_poly.end());
        auto [canonical, reversed] = detail::canonicalize_polygon(polygons[fi]);
        face_groups[sorted_poly].push_back({fi, reversed});
    }

    std::vector<bool> remove_face(polygons.size(), false);

    // 3. Process each group: Annihilate antiparallel hangnails & merge redundant duplicates
    for (const auto& [sorted_poly, instances] : face_groups) {
        if (instances.size() < 2) continue;

        std::vector<std::size_t> forward_faces;
        std::vector<std::size_t> reversed_faces;
        for (const auto& [fi, reversed] : instances) {
            if (!reversed) forward_faces.push_back(fi);
            else reversed_faces.push_back(fi);
        }

        // Case A: Antiparallel Pair (Both forward and reversed sheets present)
        if (!forward_faces.empty() && !reversed_faces.empty()) {
            std::size_t pairs_to_annihilate = std::min(forward_faces.size(), reversed_faces.size());
            for (std::size_t p = 0; p < pairs_to_annihilate; ++p) {
                remove_face[forward_faces[p]] = true;
                remove_face[reversed_faces[p]] = true;
            }
            forward_faces.erase(forward_faces.begin(), forward_faces.begin() + pairs_to_annihilate);
            reversed_faces.erase(reversed_faces.begin(), reversed_faces.begin() + pairs_to_annihilate);
        }

        // Case B: Same-Orientation Redundant Duplicates (Keep 1, remove extras)
        for (std::size_t i = 1; i < forward_faces.size(); ++i) {
            remove_face[forward_faces[i]] = true;
        }
        for (std::size_t i = 1; i < reversed_faces.size(); ++i) {
            remove_face[reversed_faces[i]] = true;
        }
    }

    // 4. Iterative Valency Pruning (TEMPORARILY DISABLED)
    // Valency pruning (valency <= 2) was intended to peel dangling fins from solids,
    // but unconditionally destroys legitimate 2D planar surfaces (e.g. Box(w,h,0), Disk, Fill)
    // whose boundary vertices naturally have valency 1 or 2.
    /*
    for (std::size_t fi = 0; fi < polygons.size(); ++fi) {
        if (remove_face[fi]) {
            for (auto v : polygons[fi]) {
                if (v < n_points) vertex_valency[v]--;
            }
        }
    }

    std::queue<std::size_t> low_valency_queue;
    for (std::size_t v = 0; v < n_points; ++v) {
        if (vertex_valency[v] > 0 && vertex_valency[v] <= 2) {
            low_valency_queue.push(v);
        }
    }

    std::vector<std::vector<std::size_t>> vertex_to_faces(n_points);
    for (std::size_t fi = 0; fi < polygons.size(); ++fi) {
        if (!remove_face[fi]) {
            for (auto v : polygons[fi]) {
                if (v < n_points) vertex_to_faces[v].push_back(fi);
            }
        }
    }

    while (!low_valency_queue.empty()) {
        std::size_t v = low_valency_queue.front();
        low_valency_queue.pop();

        if (vertex_valency[v] <= 0 || vertex_valency[v] > 2) continue;

        for (auto fi : vertex_to_faces[v]) {
            if (remove_face[fi]) continue;
            remove_face[fi] = true;

            for (auto neighbor_v : polygons[fi]) {
                if (neighbor_v < n_points) {
                    vertex_valency[neighbor_v]--;
                    if (vertex_valency[neighbor_v] > 0 && vertex_valency[neighbor_v] <= 2) {
                        low_valency_queue.push(neighbor_v);
                    }
                }
            }
        }
    }
    */

    // 5. Compact the polygon container
    PolygonRange cleaned_polygons;
    cleaned_polygons.reserve(polygons.size());
    for (std::size_t fi = 0; fi < polygons.size(); ++fi) {
        if (!remove_face[fi]) {
            cleaned_polygons.push_back(std::move(polygons[fi]));
        }
    }

    std::size_t removed_count = initial_face_count - cleaned_polygons.size();
    polygons = std::move(cleaned_polygons);
    return removed_count;
}

/**
 * @brief Comprehensive Solid-Aware Polygon Soup Repair Pipeline.
 * 
 * Composes CGAL's exact lexicographical point unification with JotCAD's solid-regularizing
 * paired annihilation and isolated vertex cleanup.
 * Guarantees that zero-volume hangnails are excised without creating boundary punctures.
 */
template <typename PointRange, typename PolygonRange>
inline void repair_solid_soup(PointRange& points, PolygonRange& polygons) {
    namespace PMP = CGAL::Polygon_mesh_processing;

    // Step 1: Exact point unification into unique vertex IDs
    PMP::merge_duplicate_points_in_polygon_soup(points, polygons);

    // Step 2: Discard degenerate polygons with fewer than 3 unique vertices
    polygons.erase(
        std::remove_if(polygons.begin(), polygons.end(), [](const auto& poly) {
            if (poly.size() < 3) return true;
            for (std::size_t i = 0; i < poly.size(); ++i) {
                if (poly[i] == poly[(i + 1) % poly.size()]) return true;
            }
            return false;
        }),
        polygons.end()
    );

    // Step 3: Solid Regularization (Annihilate antiparallel hangnails & deduplicate)
    regularize_solid_soup_faces(points, polygons);

    // Step 4: Sweep unreferenced / isolated vertices and compact indices
    PMP::remove_isolated_points_in_polygon_soup(points, polygons);
}

} // namespace fix
} // namespace geo
} // namespace jotcad
