#pragma once

#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polyhedron_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/convex_hull_3.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

namespace geometry {

static GeometryId Hull(Assets& assets, std::vector<Shape>& shapes) {
  std::vector<EK::Point_3> points;

  for (Shape& shape : shapes) {
    shape.Walk([&](Shape& sub_shape) {
      if (!sub_shape.HasGeometryId()) {
        return true;
      }
      Geometry geometry = assets.GetGeometry(sub_shape.GeometryId())
                              .Transform(sub_shape.GetTf());
      for (const auto& vertex : geometry.vertices_) {
        points.push_back(vertex);
      }
      return true;
    });
  }

  if (points.empty()) {
    Napi::Error::New(assets.Env(), "Hull: no points provided")
        .ThrowAsJavaScriptException();
    return assets.Env().Undefined();
  }

  CGAL::Surface_mesh<EK::Point_3> hull_mesh;
  CGAL::convex_hull_3(points.begin(), points.end(), hull_mesh);

  if (!hull_mesh.is_empty() &&
      !CGAL::Polygon_mesh_processing::is_outward_oriented(hull_mesh)) {
    Napi::Error::New(assets.Env(),
                     "Hull: generated mesh is not outwardly oriented")
        .ThrowAsJavaScriptException();
    return assets.Env().Undefined();
  }

  Geometry output;
  output.DecodeSurfaceMesh<EK>(hull_mesh);
  return assets.TextId(output);
}

}  // namespace geometry
