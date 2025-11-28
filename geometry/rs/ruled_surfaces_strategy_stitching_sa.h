#pragma once

#include <random>

#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "ruled_surfaces_strategy_simulated_annealing.h"

namespace ruled_surfaces {

namespace internal {
}  // namespace internal

// Implements a simulated annealing search strategy for finding an
// optimal triangulation for a ruled surface between two *open* polygonal chains,
// `p_open` and `q_open`, which are assumed to be pre-aligned segments of
// closed curves that have been cut at an appropriate seam.
//
// The algorithm only explores triangulation moves to find a triangulation
// that avoids error accumulation, and then stitches the ends together to
// form a closed surface.
template <typename Objective, typename StoppingRule_>
class StitchingSA {
 public:
  using objective_type = Objective;
  using StoppingRule = StoppingRule_;

  struct Options {
    StoppingRule stopping_rule;
    std::optional<unsigned int> seed;
  };

  StitchingSA(const Objective& objective, const Options& options)
      : objective_(objective), options_(options) {}

  // Executes the stitching simulated annealing algorithm.
  //
  // - p_open, q_open: The two open polygonal chains, assumed to be pre-aligned.
  // - visitor: A SolutionVisitor to process all valid solutions found.
  void generate(const PolygonalChain& p_open, const PolygonalChain& q_open,
                RuledSurfaceVisitor& visitor);

 private:
  std::optional<std::pair<Mesh, double>> run_sa(
      const PolygonalChain& p_open, const PolygonalChain& q_open,
      RuledSurfaceVisitor& visitor, std::mt19937& gen, SolutionStats& stats);
  Objective objective_;
  Options options_;
};

template <typename Objective, typename StoppingRule>
std::optional<std::pair<Mesh, double>>
StitchingSA<Objective, StoppingRule>::run_sa(const PolygonalChain& p_open,
                                             const PolygonalChain& q_open,
                                             RuledSurfaceVisitor& visitor,
                                             std::mt19937& gen,
                                             SolutionStats& stats) {
  std::uniform_real_distribution<> prob_dist(0.0, 1.0);

  // 1. Initial State
  std::vector<bool> current_moves =
      internal::get_random_initial_moves(p_open, q_open, gen);
  if (current_moves.empty()) {
    return std::nullopt;
  }

  Mesh current_open_mesh =
      internal::moves_to_mesh(p_open, q_open, current_moves);
  if(current_open_mesh.is_empty()) return std::nullopt;
  auto current_stitched = internal::stitch_and_calculate_cost(
      current_open_mesh, p_open, q_open, objective_);
  if (!current_stitched) {
    visitor.OnInvalid(current_open_mesh);
    return std::nullopt;
  }
  double current_cost = current_stitched->second;
  Mesh current_mesh = current_stitched->first;

  double best_cost = current_cost;
  Mesh best_mesh = current_mesh;
  int iters_without_improvement = 0;

  double temp = 1.0;
  double cooling_rate =
      std::pow(0.001, 1.0 / options_.stopping_rule.max_iterations());

  std::uniform_int_distribution<> move_idx_dist(0, current_moves.size() - 2);

  int i = 0;
  for (;; ++i) {
    if (options_.stopping_rule.ShouldStop(i, best_cost,
                                          iters_without_improvement)) {
      break;
    }
    
    visitor.OnSearchStep(false, temp, 0.0);

    std::vector<bool> next_moves = current_moves;

    // 2. Get neighbor via triangulation move
    int idx = move_idx_dist(gen);
    int attempts = 0;
    while (next_moves[idx] == next_moves[idx + 1] && attempts < 10) {
      idx = move_idx_dist(gen);
      attempts++;
    }
    if (next_moves[idx] != next_moves[idx + 1]) {
      bool temp = next_moves[idx];
      next_moves[idx] = next_moves[idx + 1];
      next_moves[idx + 1] = temp;
    }

    if (next_moves.empty()) continue;

    Mesh next_open_mesh =
        internal::moves_to_mesh(p_open, q_open, next_moves);
    if (next_open_mesh.is_empty()) {
      continue;
    }
    if (internal::has_self_intersections(next_open_mesh)) {
      if (visitor.OnInvalid(next_open_mesh) == RuledSurfaceVisitor::kStop)
        return std::nullopt;
      continue;
    }

    auto next_stitched = internal::stitch_and_calculate_cost(
        next_open_mesh, p_open, q_open, objective_);
    if (!next_stitched) {
      if (visitor.OnInvalid(next_open_mesh) == RuledSurfaceVisitor::kStop)
        return std::nullopt;
      continue;
    }

    Mesh& next_mesh = next_stitched->first;
    double next_cost = next_stitched->second;

    if (visitor.OnValidMesh(next_mesh, next_cost, 0) == RuledSurfaceVisitor::kStop) {
      return std::nullopt;
    }

    if (next_cost < best_cost) {
      best_cost = next_cost;
      best_mesh = next_mesh;
      iters_without_improvement = 0;
    } else {
      iters_without_improvement++;
    }

    double delta_cost = next_cost - current_cost;
    bool accepted =
        delta_cost < 0 ||
        (temp > 0 && prob_dist(gen) < std::exp(-delta_cost / temp));
    visitor.OnAcceptance(accepted, next_cost, current_cost, false);

    if (accepted) {
      current_moves = next_moves;
      current_cost = next_cost;
    }

    temp *= cooling_rate;
  }
  stats.paths_evaluated += i;
  return std::make_pair(best_mesh, best_cost);
}

template <typename Objective, typename StoppingRule>
void StitchingSA<Objective, StoppingRule>::generate(
    const PolygonalChain& p_open, const PolygonalChain& q_open,
    RuledSurfaceVisitor& visitor) {
  visitor.OnStart(p_open, q_open);
  SolutionStats stats;
  stats.paths_evaluated = 0;

  if (p_open.size() < 2 || q_open.size() < 2) {
    stats.status = SolutionStats::NO_SOLUTION_FOUND;
    visitor.OnFinish(stats);
    return;
  }

  std::mt19937 gen(options_.seed.value_or(23));

  // Run with q_open
  auto result1 = run_sa(p_open, q_open, visitor, gen, stats);

  // Run with reversed q_open
  PolygonalChain q_open_rev = q_open;
  std::reverse(q_open_rev.begin(), q_open_rev.end());
  auto result2 = run_sa(p_open, q_open_rev, visitor, gen, stats);

  if (!result1 && !result2) {
    stats.status = SolutionStats::NO_SOLUTION_FOUND;
    visitor.OnFinish(stats);
    return;
  }

  if (result1 && (!result2 || result1->second < result2->second)) {
    visitor.OnValidMesh(result1->first, result1->second, 0);
  } else if (result2) {
    visitor.OnValidMesh(result2->first, result2->second, 0);
  }
  
  stats.shift = 0;
  visitor.OnFinish(stats);
}

} // namespace ruled_surfaces

