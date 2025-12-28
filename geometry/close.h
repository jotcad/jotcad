#pragma once

#include <CGAL/Surface_mesh.h>

#include "assets.h"
#include "geometry.h"
#include "repair_util.h"
#include "shape.h"

namespace geometry {

inline GeometryId Close3(Assets& assets, Shape& shape) {
  if (!shape.HasGeometryId()) {
    Napi::Error::New(assets.Env(), "Close3: shape has no geometry")
        .ThrowAsJavaScriptException();
    return assets.UndefinedId();
  }
  GeometryId id = shape.GeometryId();
  // assets.GetSurfaceMesh returns a reference, we need a copy.
  CGAL::Surface_mesh<EK::Point_3> mesh = assets.GetSurfaceMesh(id);
  repair_close_holes<EK>(mesh);
  Geometry geometry;
  geometry.DecodeSurfaceMesh<EK>(mesh);
  return assets.TextId(geometry);
}

}  // namespace geometry