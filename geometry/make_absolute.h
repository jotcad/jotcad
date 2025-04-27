#pragma once
#include "assets.h"
#include "shape.h"
#include "geometry.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

static GeometryId MakeAbsolute(Assets& assets, Shape& shape) {
  auto id = shape.GeometryId();
  Geometry geometry = assets.GetGeometry(shape.GeometryId());
  Geometry transformed = geometry.Transform(shape.GetTf());
  auto result = assets.TextId(transformed);
  return result;
}
