#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <numeric>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>
#include <set>
#include <limits>
#include <cmath>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_multi_surface.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "visitor.h"

namespace ruled_surfaces {

template <typename T>
struct is_seam_search_sa_mj : std::false_type {};
template <typename TS, typename SR>
struct is_seam_search_sa_mj<SeamSearchSA<TS, SR>> : std::true_type {};

template <typename T>
struct is_seam_search_all_mj : std::false_type {};
template <typename TS>
struct is_seam_search_all_mj<SeamSearchAll<TS>> : std::true_type {};

// MultiJoinStrategy: supports N-to-1, 1-to-N, and N-to-N loop matching.
//
// For mismatched counts (e.g. 1 base loop -> N top loops), it rules the full
// base loop against each top loop independently and accumulates the meshes.
// The saddle crease is produced naturally by the subsequent merge_and_weld
// step in rule_op.h — no explicit partitioning needed.
template <typename SeamStrategy>
class MultiJoinStrategy {
 private:
  static PointCgal calculate_centroid(const PolygonalChain& chain) {
    double sx = 0, sy = 0, sz = 0;
    int count = 0;
    for (const auto& p : chain) {
      sx += p.x();
      sy += p.y();
      sz += p.z();
      count++;
    }
    if (count == 0) return PointCgal(0, 0, 0);
    return PointCgal(sx / count, sy / count, sz / count);
  }

  static double distance_sq(const PointCgal& p1, const PointCgal& p2) {
    double dx = p1.x() - p2.x();
    double dy = p1.y() - p2.y();
    double dz = p1.z() - p2.z();
    return dx*dx + dy*dy + dz*dz;
  }

  // Rule a single (p, q) pair using the correct strategy variant.
  static SolutionStats::Status rule_pair(
      const PolygonalChain& p,
      const PolygonalChain& q,
      const typename SeamStrategy::objective_type& objective,
      int max_paths,
      Mesh* out) {
    SolutionStats pair_stats;
    BestTriangulationSearchSolutionVisitor visitor(out, &pair_stats);

    if constexpr (is_seam_search_sa_mj<SeamStrategy>::value) {
      using TriangulationStrategy = typename SeamStrategy::triangulation_strategy_type;
      typename TriangulationStrategy::Options ls_options;
      ls_options.max_total_paths = 1;
      ls_options.stitch = true;
      TriangulationStrategy ls_strategy(objective, ls_options);
      typename SeamStrategy::Options sa_options;
      sa_options.stopping_rule = ConvergenceStoppingRule(100, max_paths);
      SeamStrategy seam_strategy(ls_strategy, sa_options);
      seam_strategy.generate(p, q, visitor);
    }
    else if constexpr (is_seam_search_all_mj<SeamStrategy>::value) {
      using TriangulationStrategy = typename SeamStrategy::triangulation_strategy_type;
      typename TriangulationStrategy::Options ls_options;
      ls_options.max_total_paths = 1;
      ls_options.stitch = true;
      TriangulationStrategy ls_strategy(objective, ls_options);
      typename SeamStrategy::Options options;
      SeamStrategy seam_strategy(ls_strategy, options);
      seam_strategy.generate(p, q, visitor);
    }
    else {
      typename SeamStrategy::Options options;
      options.max_total_paths = max_paths;
      options.stitch = false;
      SeamStrategy seam_strategy(objective, options);
      seam_strategy.generate(p, q, visitor);
    }

    return out->is_empty() ? SolutionStats::NO_SOLUTION_FOUND : SolutionStats::OK;
  }

 public:
  static SolutionStats::Status generate(
      const std::vector<PolygonalChain>& p_chains,
      const std::vector<PolygonalChain>& q_chains,
      const typename SeamStrategy::objective_type& objective,
      int max_paths_per_pair, MultiSurfaceStats* stats, Mesh* result) {
    if (stats) {
      *stats = {};
    }
    *result = Mesh{};

    const int num_p = p_chains.size();
    const int num_q = q_chains.size();

    if (num_p == 0 || num_q == 0) {
      return SolutionStats::NO_SOLUTION_FOUND;
    }

    // Case 1: Equal counts — standard 1-to-1 centroid matching (legacy path).
    if (num_p == num_q) {
      std::vector<PointCgal> p_centroids(num_p);
      for (int i = 0; i < num_p; ++i) p_centroids[i] = calculate_centroid(p_chains[i]);
      std::vector<PointCgal> q_centroids(num_q);
      for (int j = 0; j < num_q; ++j) q_centroids[j] = calculate_centroid(q_chains[j]);

      std::set<int> free_q;
      for (int j = 0; j < num_q; ++j) free_q.insert(j);

      std::vector<std::pair<int, int>> pairings;
      for (int i = 0; i < num_p; ++i) {
        int best_j = -1;
        double best_d = std::numeric_limits<double>::infinity();
        for (int j : free_q) {
          double d = distance_sq(p_centroids[i], q_centroids[j]);
          if (d < best_d) { best_d = d; best_j = j; }
        }
        if (best_j == -1) return SolutionStats::NO_SOLUTION_FOUND;
        free_q.erase(best_j);
        pairings.push_back({i, best_j});
      }

      double total_cost = 0;
      std::vector<Mesh> result_meshes;
      for (const auto& pair : pairings) {
        Mesh m;
        if (rule_pair(p_chains[pair.first], q_chains[pair.second], objective, max_paths_per_pair, &m)
            != SolutionStats::OK) {
          return SolutionStats::NO_SOLUTION_FOUND;
        }
        result_meshes.push_back(m);
      }

      if (stats) {
        stats->pairings = pairings;
      }
      result->clear();
      for (const auto& m : result_meshes) (*result) += m;
      return SolutionStats::OK;
    }

    // Case 2: 1 base loop (P) -> N top loops (Q).
    // Rule the full base loop against each top loop independently.
    // merge_and_weld in rule_op.h handles the saddle seam.
    if (num_p == 1 && num_q > 1) {
      std::vector<Mesh> result_meshes;
      for (int j = 0; j < num_q; ++j) {
        Mesh m;
        if (rule_pair(p_chains[0], q_chains[j], objective, max_paths_per_pair, &m)
            != SolutionStats::OK) {
          return SolutionStats::NO_SOLUTION_FOUND;
        }
        result_meshes.push_back(m);
      }
      result->clear();
      for (const auto& m : result_meshes) (*result) += m;
      return SolutionStats::OK;
    }

    // Case 3: N base loops (P) -> 1 top loop (Q).
    // Rule each base loop against the full top loop independently.
    if (num_p > 1 && num_q == 1) {
      std::vector<Mesh> result_meshes;
      for (int i = 0; i < num_p; ++i) {
        Mesh m;
        if (rule_pair(p_chains[i], q_chains[0], objective, max_paths_per_pair, &m)
            != SolutionStats::OK) {
          return SolutionStats::NO_SOLUTION_FOUND;
        }
        result_meshes.push_back(m);
      }
      result->clear();
      for (const auto& m : result_meshes) (*result) += m;
      return SolutionStats::OK;
    }

    return SolutionStats::NO_SOLUTION_FOUND;
  }
};

}  // namespace ruled_surfaces
