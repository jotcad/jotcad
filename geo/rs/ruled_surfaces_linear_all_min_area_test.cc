#include <cassert>
#include <iostream>

#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_strategy_linear_all.h"
#include "ruled_surfaces_test_utils.h"

namespace ruled_surfaces {
namespace test {

using namespace ruled_surfaces;

const std::string kExpectedObjMinAreaOnUnitSquare = R"(
v 0.000000 0.000000 0.000000
v 0.000000 1.000000 0.000000
v 1.000000 0.000000 0.000000
v 1.000000 1.000000 0.000000
f 1 2 3
f 3 2 4
l 1 2
l 1 3
l 2 4
)";

// Verified
void MinAreaOnUnitSquare() {
  auto [p, q] = CreateUnitSquareGeometry();
  SolutionStats stats;
  Mesh result;
  BestTriangulationSearchSolutionVisitor visitor(&result, &stats);
  LinearAllSearch<MinArea> search((MinArea()));
  assert(SolutionStats::OK == search.generate(p, q, visitor));
  visitor.OnFinish(SolutionStats{});
  assert(!result.is_empty());
  assert(result.number_of_faces() == 2);
  assert(GetMeshAsObjString(result, {p, q}) == kExpectedObjMinAreaOnUnitSquare);
  assert(visitor.best_solutions().size() == 1);
}

const std::string kExpectedObjMinAreaOnRotatedOpenCrescents = R"(
v -0.500000 -0.866025 0.000000
v 0.500000 -0.866025 1.000000
v -0.766044 -0.642788 1.000000
v -0.766044 0.642788 1.000000
v 0.766044 -0.642788 0.000000
v 0.500000 0.866025 1.000000
v 0.766044 0.642788 0.000000
v -0.500000 0.866025 0.000000
f 1 2 3
f 1 3 4
f 1 4 5
f 5 4 6
f 5 6 7
f 7 6 8
l 1 6
l 1 5 7 8
l 6 4 3 2
)";

// Verified
void MinAreaOnRotatedOpenCrescents() {
  auto [p, q] = CreateRotatedCrescentsGeometry(3);

  SolutionStats stats;
  Mesh result;
  BestTriangulationSearchSolutionVisitor visitor(&result, &stats);
  LinearAllSearch<MinArea> search((MinArea()));
  assert(SolutionStats::OK == search.generate(p, q, visitor));
  visitor.OnFinish(SolutionStats{});

  assert(!result.is_empty());
  assert(result.is_valid());
  assert(!IsSelfIntersecting(result));
  assert(GetMeshAsObjString(result, {p, q}) ==
         kExpectedObjMinAreaOnRotatedOpenCrescents);
  assert(visitor.best_solutions().size() == 10);
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
v 0.766044 0.642788 0.000000
v -0.069361 0.375697 1.000000
f 1 2 3
f 1 3 4
f 4 3 5
f 5 3 6
f 5 6 7
f 7 6 8
f 7 8 9
f 9 8 10
f 9 10 11
f 11 10 12
f 11 12 2
f 11 2 1
l 1 2
l 1 4 5 7 9 11 1
f 1 4 5 7 9 11
l 2 3 6 8 10 12 2
f 2 3 6 8 10 12
)";

// Verified
void IdeallyRotatedCrescents3() {
  auto [p, q] = CreateIdeallyRotatedClosedCrescents3Geometry();

  SolutionStats stats;
  Mesh result;
  BestTriangulationSearchSolutionVisitor visitor(&result, &stats);
  LinearAllSearch<MinArea> search((MinArea()));
  search.generate(p, q, visitor);
  assert(!result.is_empty());
  assert(SolutionStats::OK == stats.status);

  const std::string obj_string = GetMeshAsObjString(result, {p, q});
  assert(obj_string == kExpectedObjIdeallyRotatedCrescents3);
}

}  // namespace test
}  // namespace ruled_surfaces

int main(int argc, char** argv) {
  ruled_surfaces::test::MinAreaOnUnitSquare();
  ruled_surfaces::test::MinAreaOnRotatedOpenCrescents();
  ruled_surfaces::test::IdeallyRotatedCrescents3();
  return 0;
}
