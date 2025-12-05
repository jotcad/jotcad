#pragma once

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Triangle_2.h>  // Added this include

#include "assets.h"
#include "geometry.h"
#include "shape.h"

namespace geometry {

// Define a 2D kernel for footprint operations
typedef CGAL::Exact_predicates_exact_constructions_kernel EK_2D;
typedef EK_2D::Point_2 Point_2;
typedef CGAL::Polygon_2<EK_2D> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK_2D> Polygon_with_holes_2;

static GeometryId footprint(Assets& assets, Shape& shape) {
  Geometry input_geometry =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  Geometry output_geometry;

  if (input_geometry.vertices_.empty() || input_geometry.triangles_.empty()) {
    return assets.TextId(output_geometry.Transform(shape.GetTf().inverse()));
  }

  // 1. Project 3D vertices to 2D
  std::vector<Point_2> projected_points;
  // Map original 3D vertex index to new 2D vertex index
  std::map<size_t, size_t> original_to_projected_index_map;
  std::map<Point_2, size_t> projected_point_to_index_map;

  for (size_t i = 0; i < input_geometry.vertices_.size(); ++i) {
    const auto& p3 = input_geometry.vertices_[i];
    Point_2 p2(p3.x(), p3.y());

    // Check if this projected point already exists to avoid duplicates
    auto it = projected_point_to_index_map.find(p2);
    if (it == projected_point_to_index_map.end()) {
      size_t new_index = output_geometry.vertices_.size();
      output_geometry.vertices_.push_back(
          CGAL::Point_3<EK>(p3.x(), p3.y(), 0));  // Store as 3D point with z=0
      projected_points.push_back(p2);
      projected_point_to_index_map[p2] = new_index;
      original_to_projected_index_map[i] = new_index;
    } else {
      original_to_projected_index_map[i] = it->second;
    }
  }

  // 2. Create 2D polygons from projected triangles
  std::vector<Polygon_2> cgal_polygons;
  for (const auto& triangle_indices : input_geometry.triangles_) {
    // Ensure triangle is valid (3 vertices)
    if (triangle_indices.size() != 3) continue;

    // Get the projected 2D points using the new indices
    Point_2 p0 =
        projected_points[original_to_projected_index_map[triangle_indices[0]]];
    Point_2 p1 =
        projected_points[original_to_projected_index_map[triangle_indices[1]]];
    Point_2 p2 =
        projected_points[original_to_projected_index_map[triangle_indices[2]]];

    // Only add non-degenerate triangles
    EK_2D kernel;
    CGAL::Triangle_2<EK_2D> cgal_triangle(p0, p1, p2);
    if (!kernel.is_degenerate_2_object()(cgal_triangle)) {
      Polygon_2 poly;
      poly.push_back(p0);
      poly.push_back(p1);
      poly.push_back(p2);
      if (poly.is_clockwise_oriented()) {
        poly.reverse_orientation();
      }
      cgal_polygons.push_back(poly);
    }
  }

  if (cgal_polygons.empty()) {
    return assets.TextId(output_geometry.Transform(shape.GetTf().inverse()));
  }

  // 3. Compute the union of all 2D polygons using CGAL::Polygon_set_2
  // This will handle overlapping triangles and automatically find holes.
  CGAL::Polygon_set_2<EK_2D> polygon_set;
  for (const auto& poly : cgal_polygons) {
    polygon_set.join(poly);
  }

  // 4. Extract Polygon_with_holes_2 from the Polygon_set_2
  std::vector<Polygon_with_holes_2> result_pwhs;
  polygon_set.polygons_with_holes(std::back_inserter(result_pwhs));

  // 5. Convert Polygon_with_holes_2 to Geometry::faces_ structure
  for (const auto& pwh : result_pwhs) {
    std::vector<size_t> outer_face_indices;
    std::vector<std::vector<size_t>> hole_indices_list;

    // Add outer boundary vertices
    for (const auto& p2 : pwh.outer_boundary()) {
      // Find the index of this point in output_geometry.vertices_
      auto it = projected_point_to_index_map.find(p2);
      if (it != projected_point_to_index_map.end()) {
        outer_face_indices.push_back(it->second);
      } else {
        // This should theoretically not happen if projected_point_to_index_map
        // is correctly built and p2 comes from the set of projected_points. If
        // it does, it indicates a bug or floating point issue. For robustness,
        // add the point if not found.
        size_t new_index = output_geometry.vertices_.size();
        output_geometry.vertices_.push_back(
            CGAL::Point_3<EK>(p2.x(), p2.y(), 0));
        projected_point_to_index_map[p2] =
            new_index;  // Update map with new point
        outer_face_indices.push_back(new_index);
      }
    }

    // Add hole boundary vertices
    for (auto hole_it = pwh.holes_begin(); hole_it != pwh.holes_end();
         ++hole_it) {
      std::vector<size_t> current_hole_indices;
      for (const auto& p2 : *hole_it) {
        auto it = projected_point_to_index_map.find(p2);
        if (it != projected_point_to_index_map.end()) {
          current_hole_indices.push_back(it->second);
        } else {
          size_t new_index = output_geometry.vertices_.size();
          output_geometry.vertices_.push_back(
              CGAL::Point_3<EK>(p2.x(), p2.y(), 0));
          projected_point_to_index_map[p2] =
              new_index;  // Update map with new point
          current_hole_indices.push_back(new_index);
        }
      }
      hole_indices_list.push_back(current_hole_indices);
    }
    output_geometry.faces_.emplace_back(outer_face_indices, hole_indices_list);
  }

  return assets.TextId(output_geometry.Transform(shape.GetTf().inverse()));
}

}  // namespace geometry
