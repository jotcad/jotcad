#pragma once

#include <CGAL/Cartesian_converter.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/measure.h>  // For volume
#include <CGAL/Polygon_mesh_processing/orientation.h>  // For reverse_face_orientations
#include <CGAL/Polygon_mesh_processing/repair.h>  // For manifold_hole_filling
#include <CGAL/Surface_mesh.h>

#include <boost/circular_buffer.hpp>  // Required by extract_boundary_cycles
#include <boost/variant.hpp>          // Required by extract_boundary_cycles
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "assets.h"
#include "geometry.h"

// Ruled surface algorithm headers
#include "rs/objective.h"  // Added for Objective type
#include "rs/ruled_surfaces_objective_min_area.h"
#include "rs/ruled_surfaces_sa_stopping_rules.h"
#include "rs/ruled_surfaces_strategy_linear_slg.h"
#include "rs/ruled_surfaces_strategy_seam_search_sa.h"
#include "rs/solution_stats.h"
#include "rs/types.h"  // For PolygonalChain, Mesh etc.

namespace geometry {
// Bring types from ruled_surfaces into scope for convenience
using namespace ruled_surfaces;

namespace internal {

inline PolygonalChain ConvertEKPointsToIKPolygonalChain(
    const std::vector<CGAL::Point_3<EK>>& ek_points) {
  PolygonalChain chain;
  CGAL::Cartesian_converter<EK, IK> convert_ek_to_ik;
  for (const auto& ek_point : ek_points) {
    chain.push_back(convert_ek_to_ik(ek_point));
  }
  return chain;
}

void GetPolylines(Assets& assets, Shape& shape,
                  std::vector<PolygonalChain>& polylines) {
  if (shape.napi_.Has("geometry")) {
    Napi::Value id_val = shape.napi_.Get("geometry");
    if (id_val.IsString()) {
      std::string id_str = id_val.As<Napi::String>().Utf8Value();
      Geometry geom = assets.GetGeometry(id_val).Transform(shape.GetTf());
      if (!geom.vertices_.empty()) {
        polylines.push_back(
            internal::ConvertEKPointsToIKPolygonalChain(geom.vertices_));
      }
    }
  }
  if (shape.napi_.Has("shapes") && shape.napi_.Get("shapes").IsArray()) {
    Napi::Array shapes_array = shape.napi_.Get("shapes").As<Napi::Array>();
    for (uint32_t i = 0; i < shapes_array.Length(); ++i) {
      Napi::Value element = shapes_array.Get(i);
      if (element.IsObject()) {
        Shape sub_shape(element.As<Napi::Object>());
        GetPolylines(assets, sub_shape, polylines);
      }
    }
  }
}

}  // namespace internal

// Internal helper function to create a ruled surface mesh between two polylines
inline Mesh CreateRuledSurfaceMesh(
    PolygonalChain& p_in,
    PolygonalChain&
        q_in,  // Pass by ref, non-const because we might close them (push_back)
    std::optional<unsigned int> seed, uint32_t stopping_rule_max_iterations,
    uint32_t stopping_rule_iters_without_improvement) {
  // Ensure the polylines are closed.
  if (!p_in.empty() && p_in.front() != p_in.back()) {
    p_in.push_back(p_in.front());
  }
  if (!q_in.empty() && q_in.front() != q_in.back()) {
    q_in.push_back(q_in.front());
  }

  // Cleaned up Logging: Just print input polylines
  std::cout << "DEBUG: Rule - Input p_in (" << p_in.size()
            << " points):" << std::endl;
  for (const auto& pt : p_in) {
    std::cout << "  (" << pt.x() << ", " << pt.y() << ", " << pt.z() << ")"
              << std::endl;
  }
  std::cout << "DEBUG: Rule - Input q_in (" << q_in.size()
            << " points):" << std::endl;
  for (const auto& pt : q_in) {
    std::cout << "  (" << pt.x() << ", " << pt.y() << ", " << pt.z() << ")"
              << std::endl;
  }

  // LOG: Stopping rule parameters
  std::cout << "DEBUG: Rule - StoppingRule parameters: max_iterations="
            << stopping_rule_max_iterations << ", iters_without_improvement="
            << stopping_rule_iters_without_improvement << std::endl;

  // 3. Prepare parameters for AlignLoopsSA
  using TriangulationStrategy = LinearSearchSlg<MinArea>;
  using StoppingRule =
      ConvergenceStoppingRule;  // Changed from MaxIterationsStoppingRule

  typename SeamSearchSA<TriangulationStrategy, StoppingRule>::Options options;
  options.seed = seed;
  options.stopping_rule = StoppingRule(
      stopping_rule_iters_without_improvement,  // maps to convergence_iters
      stopping_rule_max_iterations);            // maps to max_iters
                                                // ConvergenceStoppingRule
  Mesh mesh_result;
  SolutionStats stats;

  // 4. Call AlignLoopsSA
  std::pair<PolygonalChain, PolygonalChain> aligned_polylines =
      AlignLoopsSA<TriangulationStrategy, StoppingRule>(
          p_in, q_in, options, &mesh_result,
          &stats);  // Pass mesh_result by pointer to be filled

  // LOG: mesh_result and SolutionStats after AlignLoopsSA
  std::cout << "DEBUG: Rule - mesh_result after AlignLoopsSA: Vertices="
            << mesh_result.number_of_vertices()
            << ", Edges=" << mesh_result.number_of_edges()
            << ", Faces=" << mesh_result.number_of_faces() << std::endl;
  std::cout << "DEBUG: Rule - SolutionStats after AlignLoopsSA:" << std::endl;
  std::cout << "  Status: " << stats.status
            << std::endl;  // Removed status_to_string()
  std::cout << "  Cost: " << stats.cost << std::endl;
  std::cout << "  Shift: " << stats.shift << std::endl;
  std::cout << "  Is Reversed: " << (stats.is_reversed ? "true" : "false")
            << std::endl;
  std::cout << "  Decision Log: " << stats.decision_log << std::endl;

  // Determine correct face orientation based on volume for a closed mesh.
  // Create a temporary mesh to close holes and compute volume without modifying
  // mesh_result initially.
  Mesh temp_mesh = mesh_result;

  // Attempt to close holes for volume computation.
  // Extract all boundary cycles.
  std::vector<Mesh::Halfedge_index> border_cycles;
  CGAL::Polygon_mesh_processing::extract_boundary_cycles(
      temp_mesh, std::back_inserter(border_cycles));

  // Fill holes for each boundary cycle.
  for (const auto& h : border_cycles) {
    std::vector<Mesh::Face_index> patch_facets;
    CGAL::Polygon_mesh_processing::triangulate_hole(
        temp_mesh, h, std::back_inserter(patch_facets));
  }
  // LOG: temp_mesh after hole filling
  std::cout << "DEBUG: Rule - temp_mesh after hole filling: Vertices="
            << temp_mesh.number_of_vertices()
            << ", Edges=" << temp_mesh.number_of_edges()
            << ", Faces=" << temp_mesh.number_of_faces() << std::endl;

  // After filling holes, compute the volume.
  // Volume can be negative if normals point inward.
  double volume = CGAL::Polygon_mesh_processing::volume(temp_mesh);

  // If volume is negative, it indicates normals are pointing inwards.
  // We want outward pointing normals, so reverse if volume is negative.
  if (volume < 0) {
    CGAL::Polygon_mesh_processing::reverse_face_orientations(mesh_result);
    std::cout << "Reversed face orientations due to negative volume."
              << std::endl;
  } else {
    std::cout << "Face orientations are outward (positive volume)."
              << std::endl;
  }

  return mesh_result;
}

// Helper to merge source mesh into target mesh
inline void MergeMesh(Mesh& target, const Mesh& source) {
  // Map vertices from source to target
  std::map<Mesh::Vertex_index, Mesh::Vertex_index> v_map;
  for (auto v : source.vertices()) {
    v_map[v] = target.add_vertex(source.point(v));
  }
  // Add faces
  for (auto f : source.faces()) {
    std::vector<Mesh::Vertex_index> face_verts;
    auto h = source.halfedge(f);
    for (auto v : source.vertices_around_face(h)) {
      face_verts.push_back(v_map[v]);
    }
    target.add_face(face_verts);
  }
}

// Single rule (for two shapes) wrapper
inline GeometryId Rule(Assets& assets, Shape& from_shape, Shape& to_shape,
                       std::optional<unsigned int> seed,
                       uint32_t stopping_rule_max_iterations,
                       uint32_t stopping_rule_iters_without_improvement) {
  std::vector<PolygonalChain> from_polylines;
  internal::GetPolylines(assets, from_shape, from_polylines);

  std::vector<PolygonalChain> to_polylines;
  internal::GetPolylines(assets, to_shape, to_polylines);

  if (from_polylines.empty() || to_polylines.empty()) {
    Napi::TypeError::New(assets.Env(),
                         "Could not extract polylines from shapes for ruling.")
        .ThrowAsJavaScriptException();
  }

  // Use the first polyline from each shape
  PolygonalChain p_in = from_polylines[0];
  PolygonalChain q_in = to_polylines[0];

  Mesh mesh_result =
      CreateRuledSurfaceMesh(p_in, q_in, seed, stopping_rule_max_iterations,
                             stopping_rule_iters_without_improvement);

  Geometry geom_result;
  geom_result.DecodeSurfaceMesh<IK>(mesh_result);
  return assets.TextId(geom_result);
}

// Overloaded Rule function for multiple shapes
inline GeometryId Rule(Assets& assets, std::vector<Shape>& shapes,
                       std::optional<unsigned int> seed,
                       uint32_t stopping_rule_max_iterations,
                       uint32_t stopping_rule_iters_without_improvement) {
  if (shapes.size() < 2) {
    Napi::TypeError::New(assets.Env(), "Rule requires at least two shapes.")
        .ThrowAsJavaScriptException();
  }

  // 1. Extract all polylines once
  std::vector<PolygonalChain> all_polylines;
  all_polylines.reserve(shapes.size());

  for (size_t i = 0; i < shapes.size(); ++i) {
    std::vector<PolygonalChain> current_polylines;
    internal::GetPolylines(assets, shapes[i], current_polylines);

    if (current_polylines.empty()) {
      Napi::TypeError::New(
          assets.Env(),
          "Could not extract polylines from one of the shapes for ruling.")
          .ThrowAsJavaScriptException();
    }
    // We take the first polyline from each shape, similar to the original logic
    all_polylines.push_back(std::move(current_polylines[0]));
  }

  Mesh accumulated_mesh;

  // 2. Iterate through pairs of polylines
  for (size_t i = 0; i < all_polylines.size() - 1; ++i) {
    // Note: CreateRuledSurfaceMesh might modify the polylines (closing them),
    // which is fine/desirable for the next iteration where all_polylines[i+1]
    // becomes p_in.
    Mesh pair_mesh = CreateRuledSurfaceMesh(
        all_polylines[i], all_polylines[i + 1], seed,
        stopping_rule_max_iterations, stopping_rule_iters_without_improvement);

    MergeMesh(accumulated_mesh, pair_mesh);
  }

  Geometry geom_result;
  geom_result.DecodeSurfaceMesh<IK>(accumulated_mesh);
  return assets.TextId(geom_result);
}

}  // namespace geometry
