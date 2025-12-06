#pragma once

#include <CGAL/Boolean_set_operations_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_set_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Triangle_2.h>

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
  CGAL::Polygon_set_2<EK_2D> final_polygon_set;

  // Lambda to get 2D footprint from a single Geometry object (base case)
  auto get_2d_footprint_from_geometry =
      [&](const Geometry& input_geom,
          CGAL::Polygon_set_2<EK_2D>& accumulator_set) {
        if (input_geom.vertices_.empty() || input_geom.triangles_.empty()) {
          return;  // No triangles to project
        }

        size_t extracted_polygons = 0;
        for (const auto& triangle_indices : input_geom.triangles_) {
          if (triangle_indices.size() != 3) continue;

          const auto& v0_3d = input_geom.vertices_[triangle_indices[0]];
          const auto& v1_3d = input_geom.vertices_[triangle_indices[1]];
          const auto& v2_3d = input_geom.vertices_[triangle_indices[2]];

          Point_2 p0(v0_3d.x(), v0_3d.y());
          Point_2 p1(v1_3d.x(), v1_3d.y());
          Point_2 p2(v2_3d.x(), v2_3d.y());

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
            accumulator_set.join(poly);
            extracted_polygons++;
          }
        }
      };

  // Local recursive lambda to process shapes
  std::function<bool(Shape&)> process_shape_recursive =
      [&](Shape& current_shape) {
        if (current_shape.HasGeometryId()) {
          Geometry input_geometry =
              assets.GetGeometry(current_shape.GeometryId())
                  .Transform(current_shape.GetTf());
          get_2d_footprint_from_geometry(input_geometry, final_polygon_set);
        }
        current_shape.ForShapes(process_shape_recursive);
        return true;  // Continue iteration
      };

  // Start recursion
  process_shape_recursive(shape);

  // Convert final_polygon_set back to Geometry::faces_ structure
  Geometry output_geometry;  // Use a fresh output geometry for final result
  std::vector<Polygon_with_holes_2> result_pwhs;
  final_polygon_set.polygons_with_holes(std::back_inserter(result_pwhs));
  output_geometry.DecodePolygonsWithHoles<EK_2D>(result_pwhs);

  return assets.TextId(output_geometry);
}
}  // namespace geometry
