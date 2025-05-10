#pragma once
#include "assets.h"
#include "geometry.h"
#include "shape.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

static GeometryId MakeAbsolute(Assets& assets, Shape& shape) {
  Geometry geometry = assets.GetGeometry(shape.GeometryId());
  Geometry transformed = geometry.Transform(shape.GetTf());
  auto result = assets.TextId(transformed);
  return result;
}
