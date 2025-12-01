#pragma once

#include <CGAL/Bbox_3.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

#include <algorithm>  // Added for std::next_permutation
#include <cassert>
#include <cstdint>
#include <numeric>  // Added for std::iota
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_multi_surface.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "visitor.h"

namespace ruled_surfaces {

template <typename T>
struct is_seam_search_sa : std::false_type {};
template <typename TS, typename SR>
struct is_seam_search_sa<SeamSearchSA<TS, SR>> : std::true_type {};

// This class implements a join strategy based on solving the linear
// assignment problem for minimum cost, where the cost of pairing two chains
// is determined by the provided SeamStrategy.
template <typename SeamStrategy>
class NaiveJoinStrategy {
 public:
  static SolutionStats::Status generate(
      const std::vector<PolygonalChain>& p_chains,
      const std::vector<PolygonalChain>& q_chains,
      const typename SeamStrategy::objective_type& objective,
      int max_paths_per_pair, MultiSurfaceStats* stats, Mesh* result);
};

template <typename SeamStrategy>
SolutionStats::Status NaiveJoinStrategy<SeamStrategy>::generate(
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

  std::vector<std::vector<Mesh>> meshes(num_p, std::vector<Mesh>(num_q));
  std::vector<std::vector<std::optional<int64_t>>> arc_costs(
      num_p, std::vector<std::optional<int64_t>>(num_q));

  for (int i = 0; i < num_p; ++i) {
    for (int j = 0; j < num_q; ++j) {
      SolutionStats pair_stats;
      Mesh pair_result;
      BestTriangulationSearchSolutionVisitor visitor(&pair_result, &pair_stats);
      if constexpr (is_seam_search_sa<SeamStrategy>::value) {
        using TriangulationStrategy =
            typename SeamStrategy::triangulation_strategy_type;
        typename TriangulationStrategy::Options ls_options;
        ls_options.max_total_paths = 1;
        TriangulationStrategy ls_strategy(objective, ls_options);
        typename SeamStrategy::Options sa_options;
        sa_options.stopping_rule =
            ConvergenceStoppingRule(100, max_paths_per_pair);
        SeamStrategy seam_strategy(ls_strategy, sa_options);
        seam_strategy.generate(p_chains[i], q_chains[j], visitor);
      } else {
        typename SeamStrategy::Options options;
        options.max_total_paths = max_paths_per_pair;
        SeamStrategy seam_strategy(objective, options);
        seam_strategy.generate(p_chains[i], q_chains[j], visitor);
      }
      if (!pair_result.is_empty()) {
        meshes[i][j] = pair_result;
        arc_costs[i][j] = static_cast<int64_t>(pair_stats.cost * 1e6);
      }
    }
  }

  std::vector<int> p_indices(num_p);
  std::iota(p_indices.begin(), p_indices.end(), 0);
  std::vector<int> q_indices(num_q);
  std::iota(q_indices.begin(), q_indices.end(), 0);

  double min_cost = std::numeric_limits<double>::infinity();
  std::vector<std::pair<int, int>> best_pairs;

  do {
    double current_cost = 0;
    std::vector<std::pair<int, int>> current_pairs;
    bool possible = true;
    for (int i = 0; i < num_p; ++i) {
      if (arc_costs[p_indices[i]][q_indices[i]]) {
        current_cost += *arc_costs[p_indices[i]][q_indices[i]];
        current_pairs.push_back({p_indices[i], q_indices[i]});
      } else {
        possible = false;
        break;
      }
    }

    if (possible && current_cost < min_cost) {
      min_cost = current_cost;
      best_pairs = current_pairs;
    }
  } while (std::next_permutation(q_indices.begin(), q_indices.end()));

  if (best_pairs.empty()) {
    return SolutionStats::NO_SOLUTION_FOUND;
  }

  std::vector<Mesh> result_meshes;
  for (const auto& p : best_pairs) {
    result_meshes.push_back(meshes[p.first][p.second]);
  }

  if (stats) {
    stats->total_cost = min_cost / 1e6;
    stats->pairings = best_pairs;
  }
  result->clear();
  for (const auto& mesh : result_meshes) (*result) += mesh;
  return SolutionStats::OK;
}
}  // namespace ruled_surfaces
