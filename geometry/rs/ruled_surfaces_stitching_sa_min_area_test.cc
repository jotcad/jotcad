<<<<<<< HEAD
#include <cassert>
#include <cmath>
#include <iostream>

=======
>>>>>>> main
#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_linear_helpers.h"
#include "ruled_surfaces_strategy_linear_slg.h"
#include "ruled_surfaces_strategy_stitching_sa.h"
#include "ruled_surfaces_tee_visitor.h"
<<<<<<< HEAD
#include "ruled_surfaces_test_utils.h"
#include "ruled_surfaces_verbose_visitor.h"
=======
#include "ruled_surfaces_verbose_visitor.h"
#include "ruled_surfaces_test_utils.h"
#include <iostream>
#include <cassert>
#include <cmath>
>>>>>>> main

namespace geometry {
namespace test {

using namespace ::geometry::test;

const std::string kExpectedObjMinAreaOnUnitCube =
<<<<<<< HEAD
    R"(
=======
R"(
>>>>>>> main
v 0.000000 0.000000 0.000000
v 0.000000 0.000000 1.000000
v 1.000000 0.000000 0.000000
v 1.000000 0.000000 1.000000
v 1.000000 1.000000 1.000000
v 1.000000 1.000000 0.000000
v 0.000000 1.000000 1.000000
v 0.000000 1.000000 0.000000
f 1 2 3
f 3 2 4
f 3 4 5
f 3 5 6
f 6 5 7
f 6 7 8
f 8 7 2
f 8 2 1
l 1 2
l 1 3 6 8
l 2 4 5 7
)";
// Verified
void MinAreaOnUnitCube() {
  auto [p, q] = CreateUnitCubeSidesGeometry();
<<<<<<< HEAD

=======
  
>>>>>>> main
  // Cut the seams to create open polylines.
  // The seam is between vertex 0 and vertex N-1.
  // For the unit cube, N=5 for each polyline.
  PolygonalChain p_open(p.begin(), p.end() - 1);
  PolygonalChain q_open(q.begin(), q.end() - 1);

  SolutionStats stats;
  Mesh result;
  BestSeamSearchVisitor visitor(&result, &stats);
  StitchingSA<MinArea, MaxIterationsStoppingRule> search(
<<<<<<< HEAD
      MinArea(), {.stopping_rule = MaxIterationsStoppingRule(2000), .seed = 0});
=======
      MinArea(),
      {.stopping_rule = MaxIterationsStoppingRule(2000), .seed = 0});
>>>>>>> main
  search.generate(p_open, q_open, visitor);
  assert(!result.is_empty());
  assert(SolutionStats::OK == stats.status);

  // For a unit cube, the minimum area surface between two opposite faces
  // would be 4.0 for rulings + 1.0 for seam = 5.0.
  // However, with seed=0, SA finds a solution with area 4.0.
  MinArea objective;
  double area = objective.calculate_cost(result);
  assert(std::abs(area - 4.0) < 1e-9);

  const std::string obj_string =
      ::geometry::test::GetMeshAsObjString(result, {p_open, q_open});
  assert(obj_string == kExpectedObjMinAreaOnUnitCube);
}

const std::string kExpectedObjRotatedCrescents3 =
<<<<<<< HEAD
    R"(
=======
R"(
>>>>>>> main
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
f 6 8 2
f 6 2 9
f 9 2 10
f 10 2 11
f 11 2 12
f 12 2 1
l 1 2
l 1 6 9 10 11 12 1
f 1 6 9 10 11 12
l 2 3 4 5 7 8 2
f 2 3 4 5 7 8
)";
void RotatedCrescents3() {
  auto [p, q] = CreateRotatedClosedCrescentsGeometry(3);

  SolutionStats stats;
  Mesh result;
  BestSeamSearchVisitor visitor(&result, &stats);
  StitchingSA<MinArea, ConvergenceStoppingRule> search(
      MinArea(),
      {.stopping_rule = ConvergenceStoppingRule(200, 2000), .seed = 0});
  search.generate(p, q, visitor);
  assert(!result.is_empty());
  assert(SolutionStats::OK == stats.status);
  const std::string obj_string =
      ::geometry::test::GetMeshAsObjString(result, {p, q});
  assert(obj_string == kExpectedObjRotatedCrescents3);
}

const std::string kExpectedObjRotatedCrescents5 = R"(
v 0.309017 0.951057 0.000000
v -0.309017 0.951057 1.000000
v -0.500000 0.866025 0.000000
v -0.105309 0.629979 0.000000
v -0.913545 0.406737 1.000000
v 0.121565 0.229945 0.000000
v 0.121565 -0.229945 0.000000
v -0.913545 -0.406737 1.000000
v -0.105309 -0.629979 0.000000
v -0.309017 -0.951057 1.000000
v -0.500000 -0.866025 0.000000
v 0.309017 -0.951057 0.000000
v 0.500000 -0.866025 1.000000
v 0.105309 -0.629979 1.000000
v 0.913545 -0.406737 0.000000
v -0.121565 -0.229945 1.000000
v -0.121565 0.229945 1.000000
v 0.913545 0.406737 0.000000
v 0.105309 0.629979 1.000000
v 0.500000 0.866025 1.000000
f 1 2 3
f 3 2 4
f 4 2 5
f 4 5 6
f 6 5 7
f 7 5 8
f 7 8 9
f 9 8 10
f 9 10 11
f 11 10 12
f 12 10 13
f 12 13 14
f 12 14 15
f 15 14 16
f 15 16 17
f 15 17 18
f 18 17 19
f 18 19 20
f 18 20 2
f 18 2 1
l 1 20
l 1 3 4 6 7 9 11 12 15 18
l 20 19 17 16 14 13 10 8 5 2
)";

void RotatedCrescents5() {
  auto [p_closed, q_closed] = CreateRotatedClosedCrescentsGeometry(5);

  typename SeamSearchSA<LinearSearchSlg<MinArea>,
<<<<<<< HEAD
                        MaxIterationsStoppingRule>::Options options = {
      .seed = 0, .stopping_rule = MaxIterationsStoppingRule(2000)};
=======
                        MaxIterationsStoppingRule>::Options
      options = {.seed = 0, .stopping_rule = MaxIterationsStoppingRule(2000)};
>>>>>>> main
  auto [p_aligned, q_aligned] =
      AlignLoopsSA<LinearSearchSlg<MinArea>, MaxIterationsStoppingRule>(
          p_closed, q_closed, options);
  PolygonalChain p_open(p_aligned.begin(), p_aligned.end() - 1);
  PolygonalChain q_open(q_aligned.begin(), q_aligned.end() - 1);

  SolutionStats stats;
  Mesh result;
  BestSeamSearchVisitor visitor(&result, &stats);
  StitchingSA<MinArea, MaxIterationsStoppingRule> search(
<<<<<<< HEAD
      MinArea(), {.stopping_rule = MaxIterationsStoppingRule(2000), .seed = 0});
=======
      MinArea(),
      {.stopping_rule = MaxIterationsStoppingRule(2000), .seed = 0});
>>>>>>> main
  search.generate(p_open, q_open, visitor);

  assert(!result.is_empty());

  const std::string obj_string =
      ::geometry::test::GetMeshAsObjString(result, {p_open, q_open});
  assert(obj_string == kExpectedObjRotatedCrescents5);
}

<<<<<<< HEAD
void AlignLoopsForMinArea() {
  auto [p_closed, q_closed] = CreateRotatedClosedCrescentsGeometry(5);
  typename SeamSearchSA<LinearSearchSlg<MinArea>,
                        MaxIterationsStoppingRule>::Options options = {
      .seed = 0, .stopping_rule = MaxIterationsStoppingRule(2000)};
=======

void AlignLoopsForMinArea() {
  auto [p_closed, q_closed] = CreateRotatedClosedCrescentsGeometry(5);
  typename SeamSearchSA<LinearSearchSlg<MinArea>,
                        MaxIterationsStoppingRule>::Options
      options = {.seed = 0, .stopping_rule = MaxIterationsStoppingRule(2000)};
>>>>>>> main
  auto [p_open, q_open] =
      AlignLoopsSA<LinearSearchSlg<MinArea>, MaxIterationsStoppingRule>(
          p_closed, q_closed, options);
  assert(!p_open.empty());
  assert(!q_open.empty());
  assert(LinearSearchSlg<MinArea>::estimate_cost(p_open, q_open, MinArea()) <
<<<<<<< HEAD
         std::numeric_limits<double>::infinity());
=======
            std::numeric_limits<double>::infinity());
>>>>>>> main
}

}  // namespace test
}  // namespace geometry

int main(int argc, char** argv) {
  geometry::test::MinAreaOnUnitCube();
  geometry::test::RotatedCrescents3();
  geometry::test::RotatedCrescents5();
  geometry::test::AlignLoopsForMinArea();
  return 0;
}
