#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Clip(Assets& assets, Shape& shape,
                       std::vector<Shape>& shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh(target_mesh);

  CGAL::Surface_mesh<CGAL::Point_3<EK>> composed_tool;
  for (Shape& shape : shapes) {
    shape.Walk([&](Shape& shape) {
      if (!shape.HasGeometryId()) {
        return true;
      }
      Geometry tool =
          assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
      tool.EncodeSurfaceMesh(tool_mesh);
      if (CGAL::is_empty(composed_tool)) {
        composed_tool = tool_mesh;
      } else if (!CGAL::Polygon_mesh_processing::corefine_and_compute_union(
                     composed_tool, tool_mesh, composed_tool,
                     CGAL::parameters::throw_on_self_intersection(true),
                     CGAL::parameters::all_default(),
                     CGAL::parameters::all_default())) {
        std::cout << "Clip: non-manifold" << std::endl;
      }
      return true;
    });
  }

  if (!CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
          target_mesh, composed_tool, target_mesh,
          CGAL::parameters::throw_on_self_intersection(true),
          CGAL::parameters::all_default(), CGAL::parameters::all_default())) {
    std::cout << "Clip: non-manifold" << std::endl;
  }

  // TODO: Preserve the non-triangle geometry.
  CGAL::Surface_mesh<CGAL::Point_3<EK>> output_mesh;
  CGAL::Polygon_mesh_processing::remesh_planar_patches(target_mesh,
                                                       output_mesh);
  CGAL::Polygon_mesh_processing::triangulate_faces(output_mesh);
  Geometry output;
  output.DecodeSurfaceMesh(output_mesh);

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}
