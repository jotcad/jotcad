#ifndef EXPERIMENTAL_USERS_SBRIAN_GEOMETRY_RULE_H_
#define EXPERIMENTAL_USERS_SBRIAN_GEOMETRY_RULE_H_

#include <CGAL/Surface_mesh.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Cartesian_converter.h>
#include <CGAL/Polygon_mesh_processing/repair.h>    // For manifold_hole_filling
#include <CGAL/Polygon_mesh_processing/measure.h>   // For volume
#include <CGAL/Polygon_mesh_processing/orientation.h> // For reverse_face_orientations
#include <boost/circular_buffer.hpp>         // Required by extract_boundary_cycles
#include <boost/variant.hpp>                 // Required by extract_boundary_cycles
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <iostream>

#include "assets.h"
#include "geometry.h"

// Ruled surface algorithm headers
#include "rs/objective.h" // Added for Objective type
#include "rs/solution_stats.h"
#include "rs/types.h" // For PolygonalChain, Mesh etc.
#include "rs/ruled_surfaces_objective_min_area.h"
#include "rs/ruled_surfaces_sa_stopping_rules.h"
#include "rs/ruled_surfaces_strategy_linear_slg.h"
#include "rs/ruled_surfaces_strategy_seam_search_sa.h"

namespace geometry {
// Bring types from ruled_surfaces into scope for convenience
using namespace ruled_surfaces;

namespace internal {

inline PolygonalChain ConvertEKPointsToIKPolygonalChain(const std::vector<CGAL::Point_3<EK>>& ek_points) {
  PolygonalChain chain;
  CGAL::Cartesian_converter<EK, IK> convert_ek_to_ik;
  for (const auto& ek_point : ek_points) {
    chain.push_back(convert_ek_to_ik(ek_point));
  }
  return chain;
}

} // namespace internal


inline GeometryId Rule(
    Assets& assets,
    Shape& from_shape,
    Shape& to_shape,
    std::optional<unsigned int> seed,
    uint32_t stopping_rule_max_iterations,
    uint32_t stopping_rule_iters_without_improvement) {

  // 1. Retrieve Geometry objects from Assets and apply their transformations
  Geometry from_geom = assets.GetGeometry(from_shape.GeometryId()).Transform(from_shape.GetTf());
  Geometry to_geom = assets.GetGeometry(to_shape.GeometryId()).Transform(to_shape.GetTf());

  // 2. Convert EK points from Geometry objects to IK PolygonalChains
  PolygonalChain p_in = internal::ConvertEKPointsToIKPolygonalChain(from_geom.vertices_);
  PolygonalChain q_in = internal::ConvertEKPointsToIKPolygonalChain(to_geom.vertices_);

  // 3. Prepare parameters for RuleLoopsSA
  using TriangulationStrategy = LinearSearchSlg<MinArea>;
  using StoppingRule = ConvergenceStoppingRule;

  typename SeamSearchSA<TriangulationStrategy, StoppingRule>::Options options;
  options.seed = seed;
  options.stopping_rule = StoppingRule(stopping_rule_max_iterations, stopping_rule_iters_without_improvement);

  Mesh mesh_result;
  PolygonalChain p_aligned, q_aligned;
  SolutionStats stats; // Still needed for RuleLoopsSA call, but not stored.

  // 4. Call RuleLoopsSA
  SolutionStats::Status rule_status = RuleLoopsSA<TriangulationStrategy, StoppingRule>(
      p_in, q_in, options, &mesh_result, &p_aligned, &q_aligned, &stats);

  // Set the status in stats object explicitly as RuleLoopsSA returns it separately
  stats.status = rule_status;

  // Determine correct face orientation based on volume for a closed mesh.
  // Create a temporary mesh to close holes and compute volume without modifying mesh_result initially.
  Mesh temp_mesh = mesh_result;

  // Attempt to close holes for volume computation.
  // Extract all boundary cycles.
  std::vector<Mesh::Halfedge_index> border_cycles;
  CGAL::Polygon_mesh_processing::extract_boundary_cycles(temp_mesh, std::back_inserter(border_cycles));

  // Fill holes for each boundary cycle.
  for (const auto& h : border_cycles) {
      std::vector<Mesh::Face_index>  patch_facets;
      CGAL::Polygon_mesh_processing::triangulate_hole(
          temp_mesh, h, std::back_inserter(patch_facets));
  }


  // After filling holes, compute the volume.
  // Volume can be negative if normals point inward.
  double volume = CGAL::Polygon_mesh_processing::volume(temp_mesh);

  // If volume is negative, it indicates normals are pointing inwards.
  // We want outward pointing normals, so reverse if volume is negative.
  if (volume < 0) {
      CGAL::Polygon_mesh_processing::reverse_face_orientations(mesh_result);
      std::cout << "Reversed face orientations due to negative volume." << std::endl;
  } else {
      std::cout << "Face orientations are outward (positive volume)." << std::endl;
  }


  // 5. Convert mesh_result to Geometry and store in Assets
  Geometry geom_result;
  geom_result.DecodeSurfaceMesh<IK>(mesh_result);
  GeometryId mesh_id = assets.TextId(geom_result);
  
  // Do NOT store p_aligned, q_aligned, or stats in Assets, as per user's instruction.

  return mesh_id;
}

} // namespace geometry

#endif // EXPERIMENTAL_USERS_SBRIAN_GEOMETRY_RULE_H_
