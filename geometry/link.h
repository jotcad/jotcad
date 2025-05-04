#pragma once
#include "assets.h"
#include "shape.h"
#include "geometry.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

static GeometryId Link(Assets& assets, std::vector<Shape>& shapes, bool close, bool reverse) {
  bool first = true;
  size_t last_vertex_id;
  Geometry linked;
  for (auto& shape: shapes) {
    Geometry geometry = assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    for (const auto& vertex : geometry.vertices_) {
      size_t vertex_id = linked.AddVertex(vertex);
      if (first) {
        first = false;
      } else {
        linked.segments_.emplace_back(last_vertex_id, vertex_id);
      }
      last_vertex_id = vertex_id;
    }
  }
  if (close && !linked.segments_.empty()) {
    linked.segments_.emplace_back(last_vertex_id, 0);
  }
  if (reverse) {
    std::reverse(linked.segments_.begin(), linked.segments_.end());
    for (auto& segment : linked.segments_) {
      auto source = segment.first;
      auto target = segment.second;
      segment = std::make_pair(target, source);
    }
  }
  return assets.TextId(linked);
}
