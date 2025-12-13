#pragma once

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static bool Test(Assets& assets, Shape& shape, bool checkBoundAVolume,
                 bool checkNotSelfIntersect, bool checkIsClosed) {
  CGAL::Surface_mesh<EK::Point_3> mesh;
  shape.Walk([&](Shape& sub_shape) {
    if (!sub_shape.HasGeometryId()) {
      return true;
    }
    Geometry initial_tool =
        assets.GetGeometry(sub_shape.GeometryId()).Transform(sub_shape.GetTf());
    initial_tool.EncodeSurfaceMesh<EK>(mesh);
    return false;
  });

  if (checkBoundAVolume) {
    if (!PMP::does_bound_a_volume(mesh)) {
      return false;
    }
  }
  if (checkNotSelfIntersect) {
    if (PMP::does_self_intersect(mesh)) {
      return false;
    }
  }
  if (checkIsClosed) {
    if (!CGAL::is_closed(mesh)) {
      return false;
    }
  }

  return true;
}
