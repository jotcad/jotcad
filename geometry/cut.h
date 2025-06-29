#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Cut(Assets& assets, Shape& shape,
                      std::vector<Shape>& tool_shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh<EK>(target_mesh);

  std::function<bool(Shape&)> walk = [&](Shape& tool_shape) {
    Shape mask_shape;
    if (tool_shape.GetMask(mask_shape)) {
      // Use any mask to cut.
      walk(mask_shape);
    } else if (tool_shape.HasGeometryId()) {
      Geometry tool =
          assets.GetGeometry(tool_shape.GeometryId()).Transform(tool_shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
      tool.EncodeSurfaceMesh<EK>(tool_mesh);
      if (!CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
              target_mesh, tool_mesh, target_mesh,
              CGAL::parameters::throw_on_self_intersection(true),
              CGAL::parameters::all_default(),
              CGAL::parameters::all_default())) {
        std::cout << "Cut: non-manifold" << std::endl;
      }
      tool_shape.ForShapes(walk);
    }
    return true;
  };

  for (Shape& tool_shape : tool_shapes) {
    walk(tool_shape);
  }

  // TODO: Preserve the non-triangle geometry.
  CGAL::Surface_mesh<CGAL::Point_3<EK>> output_mesh;
  CGAL::Polygon_mesh_processing::remesh_planar_patches(target_mesh,
                                                       output_mesh);
  CGAL::Polygon_mesh_processing::triangulate_faces(output_mesh);
  Geometry output;
  output.DecodeSurfaceMesh<EK>(output_mesh);

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}
