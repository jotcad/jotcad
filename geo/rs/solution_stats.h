#pragma once

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "types.h"

namespace ruled_surfaces {

struct SolutionStats {
  enum Status { OK, PATH_LIMIT_EXCEEDED, NO_SOLUTION_FOUND };
  Status status = OK;
  int paths_evaluated = 0;
  bool path_limit_reached = false;
  bool min_area_fallback_occurred = false;
  int shift = -1;
  bool is_reversed = false;
  double cost = std::numeric_limits<double>::infinity();
  std::vector<std::pair<PointCgal, PointCgal>> rulings;
  std::string decision_log;
};

}  // namespace ruled_surfaces
