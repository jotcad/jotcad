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

  return mesh_result;
}

// Helper to merge source mesh into target mesh with stitching and orientation
// check
inline void MergeMesh(Mesh& target, const Mesh& source,
                      std::map<IK::Point_3, Mesh::Vertex_index>& vertex_map) {
  // Map vertices from source to target
  std::map<Mesh::Vertex_index, Mesh::Vertex_index> v_map;
  for (auto v : source.vertices()) {
    const auto& p = source.point(v);
    auto it = vertex_map.find(p);
    if (it == vertex_map.end()) {
      auto new_v = target.add_vertex(p);
      vertex_map[p] = new_v;
      v_map[v] = new_v;
    } else {
      v_map[v] = it->second;
    }
  }
  // Add faces
  for (auto f : source.faces()) {
    std::vector<Mesh::Vertex_index> face_verts;
    auto h = source.halfedge(f);
    for (auto v : source.vertices_around_face(h)) {
      face_verts.push_back(v_map[v]);
    }
    if (target.add_face(face_verts) == Mesh::null_face()) {
      std::reverse(face_verts.begin(), face_verts.end());
      target.add_face(face_verts);
    }
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
  std::map<IK::Point_3, Mesh::Vertex_index> vertex_map;

  // 2. Iterate through pairs of polylines
  for (size_t i = 0; i < all_polylines.size() - 1; ++i) {
    Mesh pair_mesh = CreateRuledSurfaceMesh(
        all_polylines[i], all_polylines[i + 1], seed,
        stopping_rule_max_iterations, stopping_rule_iters_without_improvement);

    MergeMesh(accumulated_mesh, pair_mesh, vertex_map);
  }

  // Final orientation fix: ensure it points outward if closed.
  if (!accumulated_mesh.is_empty()) {
    Mesh temp_mesh = accumulated_mesh;
    std::vector<Mesh::Halfedge_index> border_cycles;
    CGAL::Polygon_mesh_processing::extract_boundary_cycles(
        temp_mesh, std::back_inserter(border_cycles));
    for (const auto& h : border_cycles) {
      std::vector<Mesh::Face_index> patch_facets;
      CGAL::Polygon_mesh_processing::triangulate_hole(
          temp_mesh, h, std::back_inserter(patch_facets));
    }
    if (CGAL::Polygon_mesh_processing::volume(temp_mesh) < 0) {
      CGAL::Polygon_mesh_processing::reverse_face_orientations(
          accumulated_mesh);
    }
  }

  Geometry geom_result;
  geom_result.DecodeSurfaceMesh<IK>(accumulated_mesh);
  return assets.TextId(geom_result);
}

}  // namespace geometry
