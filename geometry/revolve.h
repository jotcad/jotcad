#pragma once

#include <cmath>
#include <iostream>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "rule.h"
#include "shape.h"
#include "sweep_util.h"

namespace geometry {

template <typename K>
static GeometryId Revolve(Assets& assets, std::vector<Shape>& shapes,
                          double angle_tau, int resolution) {
  if (shapes.empty()) return assets.UndefinedId();

  // 1. Extract Profile
  std::vector<EK::Point_3> profile;
  for (auto& shape : shapes) {
    if (!shape.HasGeometryId()) continue;
    Geometry geometry =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    for (const auto& vertex : geometry.vertices_) {
      profile.push_back(vertex);
    }
  }
  if (profile.size() < 2) return assets.UndefinedId();

  // 2. Generate Path (Circle) and Frames
  std::vector<EK::Point_3> path;
  int steps = resolution;
  if (steps < 3) steps = 3;

  // For revolution, we rotate around Z. We'll use a virtual path at radius 0?
  // Actually, let's just generate rotated profiles directly since it's a
  // circle. But we need to handle miter clipping. In revolution, every slice
  // plane P_i and P_{i+1} intersect at the Z-axis.

  bool is_full_circle = (std::abs(angle_tau - 1.0) < 1e-9);
  int num_slices = is_full_circle ? steps : steps + 1;

  std::vector<PolygonalChain> slices;
  slices.reserve(num_slices);

  for (int i = 0; i < num_slices; ++i) {
    double angle = angle_tau * 2.0 * M_PI * i / steps;
    double cos_a = std::cos(angle);
    double sin_a = std::sin(angle);

    // Calculate miter planes for this slice.
    // Slices i-1 and i intersect at Z-axis.
    // Slices i and i+1 intersect at Z-axis.
    // The "forward" direction for slice i is determined by the wedge it
    // occupies. Actually, for revolution, we just need to clip to the
    // half-plane that contains the profile and is bounded by the Z-axis.

    PolygonalChain slice;
    for (const auto& p : profile) {
      // Rotate profile point
      double x =
          CGAL::to_double(p.x()) * cos_a - CGAL::to_double(p.y()) * sin_a;
      double y =
          CGAL::to_double(p.x()) * sin_a + CGAL::to_double(p.y()) * cos_a;
      double z = CGAL::to_double(p.z());

      EK::Point_3 pt(x, y, z);

      // Truncate at Z-axis:
      // If pt is on the 'wrong' side of the axis (relative to original
      // profile), we should technically clip it. But for revolution, points are
      // usually on one side. If they cross, we snap them to the axis.

      // Simple snapping to Z-axis for points that are very close or 'cross' it.
      // Original profile is typically in +X.
      // Let's check against the original x position.
      // If original x was > 0 and pt has passed through the center, we snap.
      // This is exactly "pivoting about the extremity".

      if (CGAL::to_double(p.x()) < 0) {
        // If profile is on left, we'd snap differently.
        // For now, let's just merge points exactly on axis.
      }

      if (std::abs(x) < 1e-9 && std::abs(y) < 1e-9) {
        slice.push_back(EK::Point_3(0, 0, z));
      } else {
        slice.push_back(pt);
      }
    }
    slices.push_back(std::move(slice));
  }

  // 3. Connect slices using Rule logic
  Mesh accumulated_mesh;
  std::map<IK::Point_3, Mesh::Vertex_index> vertex_map;

  for (size_t i = 0; i < slices.size() - 1; ++i) {
    // Use Rule triangulation between slice i and i+1
    PolygonalChain p_in =
        internal::ConvertEKPointsToIKPolygonalChain(slices[i]);
    PolygonalChain q_in =
        internal::ConvertEKPointsToIKPolygonalChain(slices[i + 1]);

    // If we want a solid revolution, we'd need to close the loops or handle
    // open surfaces. Creating a ruled surface mesh between them:
    Mesh pair_mesh = CreateRuledSurfaceMesh(p_in, q_in, std::nullopt, 20, 10);
    MergeMesh(accumulated_mesh, pair_mesh, vertex_map);
  }

  if (is_full_circle) {
    PolygonalChain p_in =
        internal::ConvertEKPointsToIKPolygonalChain(slices.back());
    PolygonalChain q_in =
        internal::ConvertEKPointsToIKPolygonalChain(slices.front());
    Mesh pair_mesh = CreateRuledSurfaceMesh(p_in, q_in, std::nullopt, 20, 10);
    MergeMesh(accumulated_mesh, pair_mesh, vertex_map);
  }

  Geometry output;
  output.DecodeSurfaceMesh<IK>(accumulated_mesh);
  return assets.TextId(output);
}

}  // namespace geometry