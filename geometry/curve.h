#pragma once

#include <cmath>
#include <stdexcept>
#include <vector>

#include "assets.h"
#include "catmull_rom_util.h"
#include "geometry.h"
#include "shape.h"

namespace geometry {

template <typename K>
static GeometryId Curve(Assets& assets, std::vector<Shape>& points, bool closed,
                        double resolution) {
  std::vector<typename K::Point_3> input_points;

  for (auto& shape : points) {
    if (!shape.HasGeometryId()) continue;

    Geometry geom =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

    for (const auto& v : geom.vertices_) {
      CGAL::Cartesian_converter<EK, K> to_K;
      input_points.push_back(to_K(v));
    }
  }

  if (input_points.size() < 2) {
    throw std::runtime_error("Curve requires at least 2 points.");
  }

  std::vector<typename K::Point_3> curve_points;

  std::vector<typename K::Point_3> padded_points = input_points;
  if (closed) {
    padded_points.insert(padded_points.begin(), input_points.back());
    padded_points.push_back(input_points.front());
    padded_points.push_back(input_points[1]);
  } else {
    padded_points.insert(padded_points.begin(), input_points.front());
    padded_points.push_back(input_points.back());
  }

  int steps = static_cast<int>(resolution);
  if (steps < 1) steps = 10;

  for (size_t i = 0; i < padded_points.size() - 3; ++i) {
    const auto& p0 = padded_points[i];
    const auto& p1 = padded_points[i + 1];
    const auto& p2 = padded_points[i + 2];
    const auto& p3 = padded_points[i + 3];

    for (int j = 0; j < steps; ++j) {
      double t = (double)j / steps;
      curve_points.push_back(catmull_rom<K>(p0, p1, p2, p3, t));
    }
  }

  if (closed) {
    curve_points.push_back(curve_points.front());
  } else {
    curve_points.push_back(input_points.back());
  }

  Geometry geom;
  size_t last_idx = -1;
  bool first = true;

  for (const auto& p : curve_points) {
    size_t idx = geom.AddVertex(p);
    if (!first) {
      geom.segments_.emplace_back(last_idx, idx);
    }
    last_idx = idx;
    first = false;
  }

  return assets.TextId(geom);
}

}  // namespace geometry