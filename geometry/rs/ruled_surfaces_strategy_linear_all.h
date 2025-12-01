#pragma once

#include "ruled_surfaces_strategy_all_helpers.h"

namespace ruled_surfaces {

template <typename Objective>
class LinearAllSearch {
 public:
  using objective_type = Objective;

  LinearAllSearch(const Objective& objective) : objective_(objective) {}

  SolutionStats::Status generate(const PolygonalChain& p,
                                 const PolygonalChain& q,
                                 RuledSurfaceVisitor& visitor);
 private:
  Objective objective_;
};

template <typename Objective>
SolutionStats::Status LinearAllSearch<Objective>::generate(
    const PolygonalChain& p, const PolygonalChain& q,
    RuledSurfaceVisitor& visitor) {
  if (visitor.OnSeam(false) != RuledSurfaceVisitor::kStop) {
    internal::triangulate_exhaustive(p, q, objective_, visitor);
  }
  PolygonalChain q_rev = q;
  std::reverse(q_rev.begin(), q_rev.end());
  if (visitor.OnSeam(true) != RuledSurfaceVisitor::kStop) {
    internal::triangulate_exhaustive(p, q_rev, objective_, visitor);
  }

  return SolutionStats::OK;
}

} // namespace ruled_surfaces
