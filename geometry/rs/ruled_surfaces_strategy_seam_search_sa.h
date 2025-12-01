#pragma once

#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "visitor.h"

namespace ruled_surfaces {

namespace internal {
// Captures cost from OnShift.
class ShiftCostVisitor : public RuledSurfaceVisitor {
 public:
  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    if (cost < cost_) {
      cost_ = cost;
    }
    status_ = SolutionStats::OK;
    return kContinue;
  }
  double cost_ = std::numeric_limits<double>::infinity();
  SolutionStats::Status status_ = SolutionStats::NO_SOLUTION_FOUND;
};

// Visitor used by AlignLoopsSA to capture aligned polylines and best mesh.
class AlignPolylinesVisitor : public RuledSurfaceVisitor {
 public:
  AlignPolylinesVisitor(PolygonalChain& p_aligned, PolygonalChain& q_aligned,
                        Mesh* mesh, SolutionStats* stats = nullptr)
      : p_aligned_(p_aligned),
        q_aligned_(q_aligned),
        mesh_(mesh),
        stats_(stats) {}
  VisitControl OnValidPolylines(const PolygonalChain& p_aligned,
<<<<<<< HEAD
                                const PolygonalChain& q_aligned) override {
=======
                                    const PolygonalChain& q_aligned) override {
>>>>>>> main
    p_aligned_ = p_aligned;
    q_aligned_ = q_aligned;
    return kContinue;
  }
  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    if (mesh_ != nullptr && cost < best_cost_) {
      *mesh_ = mesh;
      best_cost_ = cost;
    }
    return kContinue;
  }
  void OnFinish(const SolutionStats& stats) override {
    if (stats_) {
      *stats_ = stats;
      stats_->shift = seam_shift_;
      stats_->is_reversed = seam_is_reversed_;
      std::stringstream ss;
      ss << "shift=" << stats_->shift << ", reversed=" << stats_->is_reversed;
      stats_->decision_log = ss.str();
    }
    status_ = stats.status;
  }

  int seam_shift_ = -1;
  bool seam_is_reversed_ = false;
  PolygonalChain& p_aligned_;
  PolygonalChain& q_aligned_;
  Mesh* mesh_ = nullptr;
  SolutionStats* stats_ = nullptr;
  double best_cost_ = std::numeric_limits<double>::infinity();
  SolutionStats::Status status_ = SolutionStats::NO_SOLUTION_FOUND;
};
}  // namespace internal

// A search strategy that uses Simulated Annealing to find an optimal seam (or
// "shift") between two loops.
//
// Unlike exhaustive methods, this strategy intelligently samples the search
// space of possible shifts. For each candidate shift, it relies on a fast
// "cost estimator" function to gauge its quality without performing a full,
// expensive triangulation. This makes it suitable for high-resolution
// geometries where an exhaustive search would be impractical.
//
// The best shift found by this search can then be passed to a separate, more
// thorough triangulation strategy (like LinearSearchSA or LinearSearchDLG) to
// generate the final surface.
template <typename TriangulationStrategy, typename StoppingRule_>
class SeamSearchSA {
 public:
  using objective_type = typename TriangulationStrategy::objective_type;
  using triangulation_strategy_type = TriangulationStrategy;
  using StoppingRule = StoppingRule_;
  struct Options {
    std::optional<unsigned int> seed;
    StoppingRule stopping_rule{100, 1000};
  };

  SeamSearchSA(const TriangulationStrategy& strategy, const Options& options)
      : strategy_(strategy), options_(options) {}

  // Executes the seam search algorithm.
  //
  // - p, q: The two loops.
  // - stopping_rule: Determines when to terminate the SA search for a seam.
  // - cost_estimator: A function to evaluate the quality of each seam.
  // - visitor: A SolutionVisitor to report progress and final results.
  // - options: Configuration for the search.
  void generate(const PolygonalChain& p, const PolygonalChain& q,
                RuledSurfaceVisitor& visitor) {
    visitor.OnStart(p, q);
    SolutionStats final_stats;

    if (p.size() < 2 || q.size() < 2 || p.front() != p.back() ||
        q.front() != q.back()) {
      final_stats.shift = 0;
      visitor.OnFinish(final_stats);
      return;
    }

    // IMPORTANT: The input loops p and q are closed loops (p.front() ==
    // p.back()), and they must be passed as *closed* loops to the
    // triangulation strategy. Do NOT remove the last point under the
    // mistaken belief that the strategy requires open loops.
    PolygonalChain q_rev = q;
    std::reverse(q_rev.begin(), q_rev.end());

    const int num_shifts = p.size() - 1;

    std::mt19937 gen(options_.seed.value_or(23));
    std::uniform_int_distribution<> shift_dist(0, num_shifts - 1);
    std::uniform_real_distribution<> prob_dist(0.0, 1.0);
    std::uniform_int_distribution<> bool_dist(0, 1);

    int best_shift = -1;
    bool best_is_reversed = false;
    double best_cost = std::numeric_limits<double>::infinity();
    int iters_without_improvement = 0;

    auto call_strategy = [&](int shift, bool is_reversed) {
      PolygonalChain p_shifted = internal::rotate_chain(p, shift);
      internal::ShiftCostVisitor cost_visitor;
      strategy_.generate(p_shifted, is_reversed ? q_rev : q, cost_visitor);
      return cost_visitor;
    };

    int current_shift = 0;
    bool current_is_reversed = false;
    double current_cost = std::numeric_limits<double>::infinity();
    internal::ShiftCostVisitor current_cost_visitor =
        call_strategy(current_shift, current_is_reversed);
    if (current_cost_visitor.status_ != SolutionStats::NO_SOLUTION_FOUND) {
      current_cost = current_cost_visitor.cost_;
    }
    if (current_cost < best_cost) {
      best_cost = current_cost;
      best_shift = current_shift;
      best_is_reversed = current_is_reversed;
    }

    double temp = 1.0;
    double cooling_rate =
        std::pow(0.001, 1.0 / options_.stopping_rule.max_iterations());
    const double kReversalMoveProb = 0.2;

    int i = 0;
    for (;; ++i) {
      if (options_.stopping_rule.ShouldStop(i, best_cost,
<<<<<<< HEAD
                                            iters_without_improvement)) {
=======
                                           iters_without_improvement)) {
>>>>>>> main
        break;
      }

      bool is_reversal_move = prob_dist(gen) < kReversalMoveProb;
      int next_shift = current_shift;
      bool next_is_reversed = current_is_reversed;

      if (is_reversal_move) {
        next_is_reversed = !current_is_reversed;
      } else {
        const int max_step =
            std::max(1, static_cast<int>(num_shifts * temp * 0.1));
        std::uniform_int_distribution<> step_dist(-max_step, max_step);
        int step = step_dist(gen);
        if (step == 0) step = (prob_dist(gen) < 0.5) ? 1 : -1;
        next_shift = (current_shift + step + num_shifts) % num_shifts;
      }

      internal::ShiftCostVisitor next_cost_visitor =
          call_strategy(next_shift, next_is_reversed);
      double next_cost =
          next_cost_visitor.status_ == SolutionStats::NO_SOLUTION_FOUND
              ? std::numeric_limits<double>::infinity()
              : next_cost_visitor.cost_;

      if (visitor.OnSeam(next_is_reversed) == RuledSurfaceVisitor::kStop) {
        break;
      }
      if (next_cost == std::numeric_limits<double>::infinity()) {
        if (current_cost == std::numeric_limits<double>::infinity()) {
          current_shift = next_shift;
          current_is_reversed = next_is_reversed;
        }
        continue;
      }

      if (next_cost < best_cost) {
        best_cost = next_cost;
        best_shift = next_shift;
        best_is_reversed = next_is_reversed;
        iters_without_improvement = 0;
      } else {
        iters_without_improvement++;
      }

      double delta_cost = next_cost - current_cost;
      if (delta_cost < 0 ||
          current_cost == std::numeric_limits<double>::infinity() ||
          (temp > 0 && prob_dist(gen) < std::exp(-delta_cost / temp))) {
        current_shift = next_shift;
        current_is_reversed = next_is_reversed;
        current_cost = next_cost;
      }
      temp *= cooling_rate;
    }

    if (best_shift == -1) {
      final_stats.status = SolutionStats::NO_SOLUTION_FOUND;
      visitor.OnFinish(final_stats);
      return;
    }
    PolygonalChain p_shifted = internal::rotate_chain(p, best_shift);
    if (auto* align_visitor =
            dynamic_cast<internal::AlignPolylinesVisitor*>(&visitor)) {
      align_visitor->seam_shift_ = best_shift;
      align_visitor->seam_is_reversed_ = best_is_reversed;
    }
    const auto& q_use = best_is_reversed ? q_rev : q;
    if (visitor.OnValidPolylines(p_shifted, q_use) ==
        RuledSurfaceVisitor::kStop)
      return;
    strategy_.generate(p_shifted, q_use, visitor);
  }

 private:
  TriangulationStrategy strategy_;
  Options options_;
};

// Aligns two loops p_closed and q_closed to find the optimal seam
// and orientation using SeamSearchSA to minimize the cost from cost_estimator.
// Returns a pair of loops (p_aligned, q_aligned) representing the
// best alignment found, such that the seam for linear processing lies between
// p_aligned[0]/q_aligned[0] and p_aligned[n-1]/q_aligned[n-1].
// If mesh_from_cost_estimator is provided, it will be populated with the
// mesh generated by the cost_estimator for the best seam found.
template <typename TriangulationStrategy, typename StoppingRule>
std::pair<PolygonalChain, PolygonalChain> AlignLoopsSA(
    const PolygonalChain& p_closed, const PolygonalChain& q_closed,
    const typename SeamSearchSA<TriangulationStrategy, StoppingRule>::Options&
        options,
    Mesh* mesh = nullptr, SolutionStats* stats = nullptr) {
  using Objective = typename TriangulationStrategy::objective_type;
  TriangulationStrategy strategy{Objective(), {1}};
  PolygonalChain p_result, q_result;
  internal::AlignPolylinesVisitor visitor(p_result, q_result, mesh, stats);
  SeamSearchSA<TriangulationStrategy, StoppingRule> search(strategy, options);
  search.generate(p_closed, q_closed, visitor);
  return {p_result, q_result};
}

// As AlignLoopSA, but returns the mesh triangulation corresponding to the
// best alignment found.
template <typename TriangulationStrategy, typename StoppingRule>
SolutionStats::Status RuleLoopsSA(
    const PolygonalChain& p_closed, const PolygonalChain& q_closed,
    const typename SeamSearchSA<TriangulationStrategy, StoppingRule>::Options&
        options,
    Mesh* result, PolygonalChain* p_result, PolygonalChain* q_result,
    SolutionStats* stats = nullptr) {
  using Objective = typename TriangulationStrategy::objective_type;
  TriangulationStrategy strategy{Objective(), {1}};
  PolygonalChain p_temp, q_temp;
  Mesh mesh_temp;
  internal::AlignPolylinesVisitor visitor(p_result ? *p_result : p_temp,
                                          q_result ? *q_result : q_temp,
<<<<<<< HEAD
                                          result ? result : &mesh_temp, stats);
=======
                                          result ? result : &mesh_temp,
                                          stats);
>>>>>>> main
  SeamSearchSA<TriangulationStrategy, StoppingRule> search(strategy, options);
  search.generate(p_closed, q_closed, visitor);

  // Mesh closing logic is not needed here. The triangulation strategy should
  // produce a closed mesh if provided with closed input loops.

  return visitor.status_;
}

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces

>>>>>>> main
