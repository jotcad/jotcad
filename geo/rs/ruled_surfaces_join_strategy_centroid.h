#pragma once

#include <CGAL/Bbox_3.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

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

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_multi_surface.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "visitor.h"

namespace ruled_surfaces {



// This class implements a join strategy that pairs loops by minimizing
// the distance between their centroids, running in O(N^2) instead of O(N!).
template <typename SeamStrategy>
class CentroidJoinStrategy {
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

    if (num_p != num_q || num_p == 0) {
      return SolutionStats::NO_SOLUTION_FOUND;
    }

    // 1. Calculate centroids for all chains
    std::vector<PointCgal> p_centroids(num_p);
    for (int i = 0; i < num_p; ++i) {
      p_centroids[i] = calculate_centroid(p_chains[i]);
    }

    std::vector<PointCgal> q_centroids(num_q);
    for (int j = 0; j < num_q; ++j) {
      q_centroids[j] = calculate_centroid(q_chains[j]);
    }

    // 2. Perform greedy matching based on centroid distances
    std::set<int> free_q;
    for (int j = 0; j < num_q; ++j) {
      free_q.insert(j);
    }

    std::vector<std::pair<int, int>> pairings;
    for (int i = 0; i < num_p; ++i) {
      int best_j = -1;
      double best_dist_sq = std::numeric_limits<double>::infinity();
      for (int j : free_q) {
        double dx = p_centroids[i].x() - q_centroids[j].x();
        double dy = p_centroids[i].y() - q_centroids[j].y();
        double dz = p_centroids[i].z() - q_centroids[j].z();
        double dist_sq = dx*dx + dy*dy + dz*dz;
        if (dist_sq < best_dist_sq) {
          best_dist_sq = dist_sq;
          best_j = j;
        }
      }
      if (best_j == -1) {
        return SolutionStats::NO_SOLUTION_FOUND;
      }
      free_q.erase(best_j);
      pairings.push_back({i, best_j});
    }

    // 3. Triangulate only the matched pairs
    double total_cost = 0;
    std::vector<Mesh> result_meshes;

    for (const auto& pair : pairings) {
      int i = pair.first;
      int j = pair.second;

      SolutionStats pair_stats;
      Mesh pair_result;
      BestTriangulationSearchSolutionVisitor visitor(&pair_result, &pair_stats);

      if constexpr (is_seam_search_sa<SeamStrategy>::value) {
        using TriangulationStrategy = typename SeamStrategy::triangulation_strategy_type;
        typename TriangulationStrategy::Options ls_options;
        ls_options.max_total_paths = 1;
        ls_options.stitch = true; 
        TriangulationStrategy ls_strategy(objective, ls_options);
        typename SeamStrategy::Options sa_options;
        sa_options.stopping_rule = ConvergenceStoppingRule(100, max_paths_per_pair);
        SeamStrategy seam_strategy(ls_strategy, sa_options);
        seam_strategy.generate(p_chains[i], q_chains[j], visitor);
      } 
      else if constexpr (is_seam_search_all<SeamStrategy>::value) {
        using TriangulationStrategy = typename SeamStrategy::triangulation_strategy_type;
        typename TriangulationStrategy::Options ls_options;
        ls_options.max_total_paths = 1;
        ls_options.stitch = true;
        TriangulationStrategy ls_strategy(objective, ls_options);
        typename SeamStrategy::Options options;
        SeamStrategy seam_strategy(ls_strategy, options);
        seam_strategy.generate(p_chains[i], q_chains[j], visitor);
      }
      else {
        typename SeamStrategy::Options options;
        options.max_total_paths = max_paths_per_pair;
        options.stitch = false; 
        SeamStrategy seam_strategy(objective, options);
        seam_strategy.generate(p_chains[i], q_chains[j], visitor);
      }

      if (pair_result.is_empty()) {
        return SolutionStats::NO_SOLUTION_FOUND;
      }
      result_meshes.push_back(pair_result);
      total_cost += pair_stats.cost;
    }

    if (stats) {
      stats->total_cost = total_cost;
      stats->pairings = pairings;
    }

    result->clear();
    for (const auto& mesh : result_meshes) {
      (*result) += mesh;
    }
    return SolutionStats::OK;
  }
};

}  // namespace ruled_surfaces
