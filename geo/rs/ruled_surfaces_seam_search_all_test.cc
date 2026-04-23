#include <cassert>
#include <iostream>

#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_slg.h"
#include "ruled_surfaces_strategy_seam_search_all.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "ruled_surfaces_test_utils.h"

namespace ruled_surfaces {
namespace test {

// Tests that SeamSearchAll finds the optimal alignment (shift=0) for
// crescent geometry that is pre-rotated to the ideal alignment.
void RotatedCrescents3MinArea() {
  auto [p, q] = CreateIdeallyRotatedClosedCrescents3Geometry();
  ruled_surfaces::Mesh mesh;
  ruled_surfaces::SolutionStats stats;
  ruled_surfaces::BestTriangulationSearchSolutionVisitor visitor(&mesh, &stats);

  ruled_surfaces::LinearSearchSlg<ruled_surfaces::MinArea> slg(
      ruled_surfaces::MinArea(), {.max_total_paths = 1});
  ruled_surfaces::SeamSearchAll<
      ruled_surfaces::LinearSearchSlg<ruled_surfaces::MinArea>>
      search(slg);

  search.generate(p, q, visitor);

  assert(stats.status == ruled_surfaces::SolutionStats::OK);
  // The input geometry is pre-aligned, so the best shift should be 0.
  assert(!stats.is_reversed);
  assert(!mesh.is_empty());
  assert(IsManifold(mesh));
  assert(!IsSelfIntersecting(mesh));
  assert(GetMeshAsObjString(mesh) == R"OBJ(
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
}  // namespace ruled_surfaces

int main(int argc, char** argv) {
  ruled_surfaces::test::RotatedCrescents3MinArea();
  return 0;
}
