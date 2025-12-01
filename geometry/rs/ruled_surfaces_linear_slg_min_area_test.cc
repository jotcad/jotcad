<<<<<<< HEAD
#include <cassert>
#include <cmath>

=======
>>>>>>> main
#include "ruled_surfaces_base.h"
#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "ruled_surfaces_strategy_linear_slg.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "ruled_surfaces_strategy_slg_helpers.h"
#include "ruled_surfaces_test_utils.h"
<<<<<<< HEAD
=======
#include <cassert>
#include <cmath>
>>>>>>> main

namespace geometry {
namespace test {

using ::geometry::test::CreateRotatedOpenCrescentsGeometry;
using ::geometry::test::CreateTwistedStarsGeometry;

const std::string kExpectedObjMinAreaOnOpenCrescents = R"(
v -0.500000 -0.866025 0.000000
v 0.500000 0.866025 1.000000
v -0.766044 0.642788 1.000000
v -0.766044 -0.642788 1.000000
v 0.500000 -0.866025 1.000000
v 0.766044 -0.642788 0.000000
v -0.069361 -0.375697 1.000000
v -0.069361 0.375697 1.000000
v 0.766044 0.642788 0.000000
v -0.500000 0.866025 0.000000
v 0.069361 0.375697 0.000000
v 0.069361 -0.375697 0.000000
f 1 2 3
f 1 3 4
f 1 4 5
f 1 5 6
f 6 5 7
f 6 7 8
f 6 8 9
f 9 8 10
f 10 8 11
f 11 8 12
l 1 2
l 1 6 9 10 11 12
l 2 3 4 5 7 8
)";

// Verified
void MinAreaOnOpenCrescents() {
  auto [p, q] = CreateRotatedOpenCrescentsGeometry(3);

  SolutionStats stats;
  Mesh result;
  BestTriangulationSearchSolutionVisitor visitor(&result, &stats);
  LinearSearchSlg<MinArea> search(MinArea(), {.max_total_paths = 100});
  search.generate(p, q, visitor);
  assert(!result.is_empty());
  assert(SolutionStats::OK == stats.status);

  const std::string obj_string =
      ::geometry::test::GetMeshAsObjString(result, {p, q});
  assert(kExpectedObjMinAreaOnOpenCrescents == obj_string);
}

const std::string kExpectedObjIdeallyRotatedCrescents3 = R"(
v -0.500000 0.866025 0.000000
v 0.500000 0.866025 1.000000
v -0.766044 0.642788 1.000000
v 0.069361 0.375697 0.000000
v 0.069361 -0.375697 0.000000
v -0.766044 -0.642788 1.000000
v -0.500000 -0.866025 0.000000
v 0.500000 -0.866025 1.000000
v 0.766044 -0.642788 0.000000
v -0.069361 -0.375697 1.000000
v -0.069361 0.375697 1.000000
v 0.766044 0.642788 0.000000
f 1 2 3
f 1 3 4
f 4 3 5
f 5 3 6
f 5 6 7
f 7 6 8
f 7 8 9
f 9 8 10
f 9 10 11
f 9 11 12
f 12 11 2
f 12 2 1
l 1 2
l 1 4 5 7 9 12 1
f 1 4 5 7 9 12
l 2 3 6 8 10 11 2
f 2 3 6 8 10 11
)";

// Verified
void IdeallyRotatedCrescents3() {
<<<<<<< HEAD
  auto [p, q] = geometry::test::CreateIdeallyRotatedClosedCrescents3Geometry();
=======
  auto [p, q] =
      geometry::test::CreateIdeallyRotatedClosedCrescents3Geometry();
>>>>>>> main

  geometry::SolutionStats stats;
  geometry::Mesh result;
  geometry::BestTriangulationSearchSolutionVisitor visitor(&result, &stats);
<<<<<<< HEAD
  geometry::LinearSearchSlg<geometry::MinArea> search(geometry::MinArea(),
                                                      {.max_total_paths = 1});
=======
  geometry::LinearSearchSlg<geometry::MinArea> search(
      geometry::MinArea(), {.max_total_paths = 1});
>>>>>>> main
  search.generate(p, q, visitor);
  assert(!result.is_empty());
  assert(geometry::SolutionStats::OK == stats.status);

  const std::string obj_string =
      ::geometry::test::GetMeshAsObjString(result, {p, q});
  assert(obj_string == kExpectedObjIdeallyRotatedCrescents3);
}

}  // namespace test
}  // namespace geometry

int main(int argc, char** argv) {
  geometry::test::MinAreaOnOpenCrescents();
  geometry::test::IdeallyRotatedCrescents3();
  return 0;
}
