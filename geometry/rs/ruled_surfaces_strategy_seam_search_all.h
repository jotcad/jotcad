#pragma once

#include <functional>
#include <limits>
#include <random>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "visitor.h"

namespace ruled_surfaces {

// A search strategy that iterates through all possible seam alignments ("shifts")
// between two loops, and for each alignment, invokes a triangulation strategy
// to find and report solutions to a visitor.
//
// The provided triangulation strategy should be designed to work on *open*
// polygonal chains, corresponding to loops that have been cut at a seam.
//
// Callers should use a visitor (e.g., BestTriangulationSearchSolutionVisitor)
// to collect the best result found across all seams and triangulations.
template <typename TriangulationStrategy>
class SeamSearchAll {
 public:
  SeamSearchAll(const TriangulationStrategy& strategy) : strategy_(strategy) {}

  // Executes the seam search algorithm.
  //
  // - p, q: The two loops.
  // - visitor: A SolutionVisitor to report progress and final results.
  void generate(const PolygonalChain& p, const PolygonalChain& q,
                RuledSurfaceVisitor& visitor) {
    visitor.OnStart(p, q);
    SolutionStats final_stats;

    if (p.size() < 2 || q.size() < 2 || p.front() != p.back() ||
        q.front() != q.back()) {
      // This strategy is only for loops. For open chains, we consider
      // shift 0 to be the only option.
      final_stats.shift = 0;
      strategy_.generate(p, q, visitor);
      visitor.OnFinish(final_stats);
      return;
    }

    for (bool is_reversed : {false, true}) {
      if (visitor.OnSeam(is_reversed) == RuledSurfaceVisitor::kStop) break;

      PolygonalChain current_q = q;
      if (is_reversed) {
        std::reverse(current_q.begin(), current_q.end());
      }
      PolygonalChain q_open_chain(current_q.begin(), current_q.end() - 1);
      const int num_shifts = p.size() - 1;

      for (int shift = 0; shift < num_shifts; ++shift) {
        if (visitor.OnSeam(is_reversed) == RuledSurfaceVisitor::kStop) {
          visitor.OnFinish(final_stats);
          return;
        }
        PolygonalChain p_shifted_closed = internal::rotate_chain(p, shift);
        PolygonalChain p_shifted_open(p_shifted_closed.begin(),
                                      p_shifted_closed.end() - 1);
        strategy_.generate(p_shifted_open, q_open_chain, visitor);
      }
    }
    visitor.OnFinish(final_stats);
  }

 private:
  TriangulationStrategy strategy_;
};

} // namespace ruled_surfaces
