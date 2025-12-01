#pragma once

#include <vector>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_multi_surface.h"

namespace ruled_surfaces {

template <typename SeamStrategy>
class JoinStrategy {
 public:
  virtual ~JoinStrategy() = default;

  virtual SolutionStats::Status generate(
      const std::vector<PolygonalChain>& p_chains,
      const std::vector<PolygonalChain>& q_chains,
      const typename SeamStrategy::objective_type& objective,
      int max_paths_per_pair, MultiSurfaceStats* stats, Mesh* result) = = 0;
};

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces
>>>>>>> main
