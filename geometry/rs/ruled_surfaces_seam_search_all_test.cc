#include <cassert>
#include <iostream>

#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_slg.h"
#include "ruled_surfaces_strategy_seam_search_all.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "ruled_surfaces_test_utils.h"

namespace geometry {
namespace test {

// Tests that SeamSearchAll finds the optimal alignment (shift=0) for
// crescent geometry that is pre-rotated to the ideal alignment.
void RotatedCrescents3MinArea() {
  auto [p, q] = test::CreateIdeallyRotatedClosedCrescents3Geometry();
  Mesh mesh;
  SolutionStats stats;
  BestTriangulationSearchSolutionVisitor visitor(&mesh, &stats);

  LinearSearchSlg<MinArea> slg(MinArea(), {.max_total_paths = 1});
  SeamSearchAll<LinearSearchSlg<MinArea>> search(slg);

  search.generate(p, q, visitor);

  assert(stats.status == SolutionStats::OK);
  // The input geometry is pre-aligned, so the best shift should be 0.
  assert(!stats.is_reversed);
  assert(!mesh.is_empty());
  assert(test::IsManifold(mesh));
  assert(!test::IsSelfIntersecting(mesh));
  assert(test::GetMeshAsObjString(mesh) == R"OBJ(
v 0.766044 -0.642788 0.000000
v 0.500000 0.866025 1.000000
v 0.766044 0.642788 0.000000
v -0.069361 0.375697 1.000000
v -0.500000 0.866025 0.000000
v 0.069361 0.375697 0.000000
v -0.069361 -0.375697 1.000000
v 0.069361 -0.375697 0.000000
v 0.500000 -0.866025 1.000000
v -0.766044 -0.642788 1.000000
v -0.500000 -0.866025 0.000000
v -0.766044 0.642788 1.000000
f 1 2 3
f 3 2 4
f 3 4 5
f 5 4 6
f 6 4 7
f 6 7 8
f 8 7 9
f 8 9 10
f 8 10 11
f 11 10 12
)OBJ");
}

}  // namespace test
}  // namespace geometry

int main(int argc, char** argv) {
  geometry::test::RotatedCrescents3MinArea();
  return 0;
}
