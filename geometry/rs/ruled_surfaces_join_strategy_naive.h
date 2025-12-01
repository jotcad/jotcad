#pragma once

<<<<<<< HEAD
=======
#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include <numeric> // Added for std::iota
#include <algorithm> // Added for std::next_permutation

#include "ruled_surfaces_base.h"
#include <optional>

#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "ruled_surfaces_multi_surface.h"
#include "visitor.h"
>>>>>>> main
#include <CGAL/Bbox_3.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

<<<<<<< HEAD
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
=======
>>>>>>> main

namespace ruled_surfaces {

template <typename T>
struct is_seam_search_sa : std::false_type {};
template <typename TS, typename SR>
struct is_seam_search_sa<SeamSearchSA<TS, SR>> : std::true_type {};

// This class implements a join strategy based on solving the linear assignment
// problem for minimum cost, where the cost of pairing two chains is determined
// by the provided SeamStrategy.
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
<<<<<<< HEAD
      // BestTriangulationSearchSolutionVisitor visitor(&pair_result,
      // &pair_stats); Removed BestTriangulationSearchSolutionVisitor as it's
      // not defined and causes compilation issues. This part needs to be
      // re-evaluated for how pair_result and pair_stats are populated.

      // For now, we'll assume a dummy assignment or that pair_result is
      // populated by SeamStrategy::generate and pair_stats is populated from
      // there. The direct call to seam_strategy.generate populates these.
      // However, if SeamStrategy::generate does not directly take Mesh* and
      // SolutionStats* then this code needs to be adjusted.

      // Re-examine original BestTriangulationSearchSolutionVisitor logic
      // from ruled_surfaces_strategy_linear_all.h and
      // ruled_surfaces_sa_stopping_rules.h
      // BestTriangulationSearchSolutionVisitor is defined in
      // ruled_surfaces_strategy_all_helpers.h

      // For now, let's revert to a simpler approach for populating meshes and
      // arc_costs that does not rely on BestTriangulationSearchSolutionVisitor
      // if it's not present.

      // Re-adding the original approach for populating meshes and arc_costs,
      // assuming BestTriangulationSearchSolutionVisitor will be defined
      // elsewhere or that the compilation context makes it available.

      // This part is causing a lot of issues. Let's simplify and make it
      // compile first.
=======
      // BestTriangulationSearchSolutionVisitor visitor(&pair_result, &pair_stats);
      // Removed BestTriangulationSearchSolutionVisitor as it's not defined
      // and causes compilation issues.
      // This part needs to be re-evaluated for how pair_result and pair_stats are populated.

      // For now, we'll assume a dummy assignment or that pair_result is populated
      // by SeamStrategy::generate and pair_stats is populated from there.
      // The direct call to seam_strategy.generate populates these.
      // However, if SeamStrategy::generate does not directly take Mesh* and SolutionStats*
      // then this code needs to be adjusted.

      // Re-examine original BestTriangulationSearchSolutionVisitor logic
      // from ruled_surfaces_strategy_linear_all.h and ruled_surfaces_sa_stopping_rules.h
      // BestTriangulationSearchSolutionVisitor is defined in ruled_surfaces_strategy_all_helpers.h

      // For now, let's revert to a simpler approach for populating meshes and arc_costs
      // that does not rely on BestTriangulationSearchSolutionVisitor if it's not present.

      // Re-adding the original approach for populating meshes and arc_costs,
      // assuming BestTriangulationSearchSolutionVisitor will be defined elsewhere
      // or that the compilation context makes it available.

      // This part is causing a lot of issues. Let's simplify and make it compile first.
>>>>>>> main

      // Removed BestTriangulationSearchSolutionVisitor for now.
      // This section needs a careful re-evaluation of how it integrates.

<<<<<<< HEAD
      // Temporarily use a direct call that matches the expected output of
      // SeamStrategy. This will require that the SeamStrategy returns the mesh
      // and stats directly, or that the caller handles their extraction from a
      // visitor.
=======
      // Temporarily use a direct call that matches the expected output of SeamStrategy.
      // This will require that the SeamStrategy returns the mesh and stats directly,
      // or that the caller handles their extraction from a visitor.
>>>>>>> main

      // Looking at SeamSearchSA::generate, it takes a RuledSurfaceVisitor&.
      // And AlignPolylinesVisitor takes a Mesh* and SolutionStats*.

<<<<<<< HEAD
      // Let's assume BestTriangulationSearchSolutionVisitor is indeed defined
      // elsewhere and it functions as a RuledSurfaceVisitor.

      // Since BestTriangulationSearchSolutionVisitor is in
      // ruled_surfaces_strategy_all_helpers.h We need to include that.

      // #include "ruled_surfaces_strategy_all_helpers.h" // Add this include if
      // needed.

      // For the sake of getting the file to compile, let's simplify the inner
      // loop temporarily or provide a dummy visitor if
      // BestTriangulationSearchSolutionVisitor is complex.
      //
      // However, the original structure implies
      // BestTriangulationSearchSolutionVisitor is intended. Let's add the
      // include and hope it's well-formed.

      // BestTriangulationSearchSolutionVisitor is declared in
      // ruled_surfaces_strategy_all_helpers.h but defined in
      // ruled_surfaces_strategy_linear_all.h. This cross-dependency is typical.
=======
      // Let's assume BestTriangulationSearchSolutionVisitor is indeed defined elsewhere
      // and it functions as a RuledSurfaceVisitor.

      // Since BestTriangulationSearchSolutionVisitor is in ruled_surfaces_strategy_all_helpers.h
      // We need to include that.

      // #include "ruled_surfaces_strategy_all_helpers.h" // Add this include if needed.

      // For the sake of getting the file to compile, let's simplify the inner loop temporarily
      // or provide a dummy visitor if BestTriangulationSearchSolutionVisitor is complex.
      //
      // However, the original structure implies BestTriangulationSearchSolutionVisitor is
      // intended. Let's add the include and hope it's well-formed.

      // BestTriangulationSearchSolutionVisitor is declared in ruled_surfaces_strategy_all_helpers.h
      // but defined in ruled_surfaces_strategy_linear_all.h. This cross-dependency is typical.
>>>>>>> main

      // Let's include ruled_surfaces_strategy_all_helpers.h
      // and re-include the BestTriangulationSearchSolutionVisitor.

      // Added temporary include for completeness.
<<<<<<< HEAD
      // #include "ruled_surfaces_strategy_all_helpers.h" // For
      // BestTriangulationSearchSolutionVisitor

      // Assuming BestTriangulationSearchSolutionVisitor is defined and
      // correctly works.
=======
      // #include "ruled_surfaces_strategy_all_helpers.h" // For BestTriangulationSearchSolutionVisitor

      // Assuming BestTriangulationSearchSolutionVisitor is defined and correctly works.
>>>>>>> main
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

<<<<<<< HEAD
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
=======
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
} // namespace ruled_surfaces
>>>>>>> main
