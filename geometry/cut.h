#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Cut(Assets& assets, Shape& shape,
                      std::vector<Shape>& shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh(target_mesh);

  for (Shape& shape : shapes) {
    Geometry tool =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
    tool.EncodeSurfaceMesh(tool_mesh);
    if (!CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
            target_mesh, tool_mesh, target_mesh,
            CGAL::parameters::throw_on_self_intersection(true),
            CGAL::parameters::all_default(), CGAL::parameters::all_default())) {
      std::cout << "Cut: non-manifold" << std::endl;
    }
  }

  // TODO: Preserve the non-triangle geometry.
  CGAL::Polygon_mesh_processing::triangulate_faces(target_mesh);
  Geometry cut;
  cut.DecodeSurfaceMesh(target_mesh);

  return assets.TextId(cut);
}
