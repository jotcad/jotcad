#pragma once

// This header defines search strategies for finding optimal ruled surfaces
// between two polygonal chains using a Simulated Annealing (SA) approach.
//
// A ruled surface between two open polygonal chains p and q can be represented
// as a triangulation. This triangulation can be mapped to a path on a grid.
// A move along the x-axis corresponds to advancing a vertex on chain p, and a
// move along the y-axis corresponds to advancing on chain q. Any valid path
// from the start to the end of both chains that doesn't cross the diagonal
// represents a valid triangulation.
//
// The simulated annealing algorithm works as follows:
// 1. Start with a random valid triangulation (a random path on the grid).
// 2. In each iteration, generate a "neighbor" triangulation by performing a
//    small perturbation, such as swapping two adjacent moves in the path. This
//    is analogous to a "flip" operation on a diagonal in the triangulation.
// 3. Calculate the "cost" (e.g., surface area) of the new triangulation.
// 4. Decide whether to accept the new triangulation:
//    - If it's better (lower cost), always accept it.
//    - If it's worse, accept it with a certain probability. This probability
//      decreases over time as the "temperature" of the system cools down. This
//      allows the search to escape local minima.
// 5. Keep track of the best solution found throughout the process.
//
// This probabilistic approach helps explore a wide range of solutions to find a
// globally optimal or near-optimal result without exhaustively checking every
// possibility.

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_all_helpers.h"

namespace ruled_surfaces {

namespace internal {

// Constructs a mesh from a triangulation represented by a sequence of "moves".
// The sequence of booleans defines a path on a grid where 'true' corresponds
// to advancing along the first polygonal chain (p) and 'false' corresponds to
// advancing along the second (q). Each move generates a triangle connecting the
// current vertices of p and q with the next vertex on the advanced chain.
//
// For example, a 'true' move from (pi, qj) goes to (pi+1, qj) and forms the
// triangle (p[i], q[j], p[i+1]). A 'false' move goes to (pi, qj+1) and forms
// the triangle (p[i], q[j], q[j+1]).
//
// Returns an empty mesh if the move sequence is invalid (e.g., goes out of
// bounds) or results in degenerate triangles.
inline Mesh moves_to_mesh(const PolygonalChain& p, const PolygonalChain& q,
                          const std::vector<bool>& moves) {
  PolygonSoup soup;
  size_t m = p.size();
  size_t n = q.size();
  if (m < 1 || n < 1) return {};
  if (m == 1 && n == 1) return {};
  int ci = 0, cj = 0;
  for (bool is_p_succeed : moves) {
    if (is_p_succeed) {
      if (ci + 1 >= m) return {};
      const auto& p1 = p[ci];
      const auto& p2 = q[cj];
      const auto& p3 = p[ci + 1];
      if (p1 == p2 || p2 == p3 || p1 == p3 || internal::is_degenerate(p1, p2, p3)) {
        return {};
      }
      soup.push_back({p1, p2, p3});
      ci++;
    } else {
      if (cj + 1 >= n) return {};
      const auto& p1 = p[ci];
      const auto& p2 = q[cj];
      const auto& p3 = q[cj + 1];
      if (p1 == p2 || p2 == p3 || p1 == p3 || internal::is_degenerate(p1, p2, p3)) {
        return {};
      }
      soup.push_back({p1, p2, p3});
      cj++;
    }
  }
  return soup_to_mesh(soup);
}

// Generates a random, valid (non-self-intersecting) initial triangulation.
// It creates a sequence of moves corresponding to a full triangulation and
// shuffles it randomly. It then checks if the resulting mesh is valid.
// It will try up to `max_tries` times to find a valid initial state.
//
// Returns a valid move sequence or an empty vector if no valid sequence could
// be found within the given attempts.
inline std::vector<bool> get_random_initial_moves(const PolygonalChain& p,
                                                  const PolygonalChain& q,
                                                  std::mt19937& g,
                                                  int max_tries = 100) {
  size_t m = p.size();
  size_t n = q.size();
  if (m < 1 || n < 1) return {};
  if (m == 1 && n == 1) return {};

  std::vector<bool> moves(m + n - 2);
  for (size_t i = 0; i < m - 1; ++i) moves[i] = true;    // p-moves
  for (size_t i = m - 1; i < m + n - 2; ++i) moves[i] = false;  // q-moves

  for (int i = 0; i < max_tries; ++i) {
    std::shuffle(moves.begin(), moves.end(), g);
    Mesh mesh = moves_to_mesh(p, q, moves);
    if (!mesh.is_empty() && !internal::has_self_intersections(mesh)) {
      return moves;
    }
  }
  return {};  // Failed to find a valid one
}
}  // namespace internal

// Implements a simulated annealing search strategy for finding an optimal ruled
// surface between two *linear* (open) polygonal chains.
template <typename Objective, typename StoppingRule_>
class LinearSearchSA {
 public:
  using objective_type = Objective;
  using StoppingRule = StoppingRule_;

  struct Options {
    StoppingRule stopping_rule;
    std::optional<unsigned int> seed;
  };

  LinearSearchSA(const Objective& objective, const Options& options)
      : objective_(objective), options_(options) {}

  // Executes the simulated annealing algorithm.
  //
  // - p, q: The two open polygonal chains.
  // - visitor: A SolutionVisitor to process valid solutions found.
  void generate(const PolygonalChain& p, const PolygonalChain& q,
                RuledSurfaceVisitor& visitor);

 private:
  Objective objective_;
  Options options_;
};

template <typename Objective, typename StoppingRule>
void LinearSearchSA<Objective, StoppingRule>::generate(
    const PolygonalChain& p, const PolygonalChain& q,
    RuledSurfaceVisitor& visitor) {
  visitor.OnStart(p, q);
  std::mt19937 gen(options_.seed.value_or(23));
  // 1. Get initial solution
  std::vector<bool> current_moves =
      internal::get_random_initial_moves(p, q, gen);
  SolutionStats stats;
  if (current_moves.empty()) {
    stats.status = SolutionStats::NO_SOLUTION_FOUND;
    visitor.OnFinish(stats);
    return;
  }

  Mesh current_mesh = internal::moves_to_mesh(p, q, current_moves);
  double current_cost = objective_.calculate_cost(current_mesh);

  std::vector<bool> best_moves = current_moves;
  double best_cost = current_cost;
  Mesh best_mesh = current_mesh;
  int iters_without_improvement = 0;

  double temp = 1.0;
  double cooling_rate =
      std::pow(0.001, 1.0 / options_.stopping_rule.max_iterations());

  std::uniform_real_distribution<> dis(0.0, 1.0);
  std::uniform_int_distribution<> move_idx_dist(0, current_moves.size() - 2);

  int i = 0;
  for (;; ++i) {
    if (options_.stopping_rule.ShouldStop(i, best_cost,
                                          iters_without_improvement)) {
      break;
    }

    visitor.OnSearchStep(/*is_seam_move=*/false, temp,
                         /*seam_move_prob=*/-1.0);
    // 2. Get neighbor
    std::vector<bool> new_moves = current_moves;
    int idx = move_idx_dist(gen);

    // Try to find a flippable pair
    int attempts = 0;
    while (new_moves[idx] == new_moves[idx + 1] && attempts < 10) {
      idx = move_idx_dist(gen);
      attempts++;
    }
    if (new_moves[idx] == new_moves[idx + 1]) continue;

    bool temp = new_moves[idx];
    new_moves[idx] = new_moves[idx + 1];
    new_moves[idx + 1] = temp;

    Mesh new_mesh = internal::moves_to_mesh(p, q, new_moves);
    if (new_mesh.is_empty() || internal::has_self_intersections(new_mesh)) {
      if (visitor.OnInvalid(new_mesh) == RuledSurfaceVisitor::kStop) return;
      continue;
    }
    if (visitor.OnPermutation(new_mesh) == RuledSurfaceVisitor::kStop) {
      visitor.OnFinish(stats);
      return;
    }

    double new_cost = objective_.calculate_cost(new_mesh);
    if (visitor.OnValidMesh(new_mesh, new_cost) == RuledSurfaceVisitor::kStop) {
      visitor.OnFinish(stats);
      return;
    }

    if (new_cost < best_cost) {
      best_cost = new_cost;
      best_mesh = new_mesh;
      iters_without_improvement = 0;
    } else {
      iters_without_improvement++;
    }

    double delta_cost = new_cost - current_cost;
    const bool accepted =
        delta_cost < 0 ||
        (temp > 0 && dis(gen) < std::exp(-delta_cost / temp));
    visitor.OnAcceptance(accepted, new_cost, current_cost,
                         /*was_seam_move=*/false);
    if (accepted) {
      current_moves = new_moves;
      current_cost = new_cost;
    }

    temp *= cooling_rate;
  }
  stats.paths_evaluated = i;

  visitor.OnValidMesh(best_mesh, best_cost);
  visitor.OnFinish(stats);
}

} // namespace ruled_surfaces


