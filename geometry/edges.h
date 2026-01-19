#pragma once

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/detect_features.h>
#include <CGAL/Surface_mesh.h>

#include <cmath>
#include <iostream>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

namespace geometry {

typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;

static GeometryId ExtractEdges(Assets& assets, Shape& shape,
                               double angle_threshold) {
  if (!shape.HasGeometryId()) return assets.UndefinedId();

  Geometry target_geom =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

  // Using IK for feature detection.
  CGAL::Surface_mesh<IK::Point_3> mesh;
  target_geom.EncodeSurfaceMesh<IK>(mesh);

  if (mesh.is_empty()) return assets.UndefinedId();

  std::cout << "ExtractEdges: angle_threshold=" << angle_threshold << " tau ("
            << angle_threshold * 360.0 << " deg)..." << std::endl;

  auto ecm =
      mesh.add_property_map<CGAL::Surface_mesh<IK::Point_3>::Edge_index, bool>(
              "e:is_feature", false)
          .first;

  // detect_sharp_edges finds edges where the angle between faces is >
  // threshold. CGAL expects degrees. Input is in tau (0..1).
  CGAL::Polygon_mesh_processing::detect_sharp_edges(
      mesh, angle_threshold * 360.0, ecm);

  // LOGGING: Print identified angles for debugging
  for (auto e : mesh.edges()) {
    auto h = mesh.halfedge(e);
    auto f1 = mesh.face(h);
    auto f2 = mesh.face(mesh.opposite(h));

    if (f1 == mesh.null_face() || f2 == mesh.null_face()) {
      // Border edges are infinitely sharp in a sense.
      std::cout << "Edge " << e << " on h" << h
                << " is border (angle=0.5 tau / 180 deg)" << std::endl;
      continue;
    }

    auto n1 = CGAL::Polygon_mesh_processing::compute_face_normal(f1, mesh);
    auto n2 = CGAL::Polygon_mesh_processing::compute_face_normal(f2, mesh);

    double dot = CGAL::to_double(n1 * n2);
    if (dot < -1.0) dot = -1.0;
    if (dot > 1.0) dot = 1.0;
    double angle_rad = std::acos(dot);
    double angle_deg = angle_rad * 180.0 / M_PI;
    double angle_tau = angle_rad / (2.0 * M_PI);

    if (ecm[e]) {
      std::cout << "Edge " << e << " on h" << h << " angle=" << angle_tau
                << " tau (" << angle_deg << " deg) [Sharp!]" << std::endl;
    }
  }

  Geometry output;
  // We want to return these as segments/polylines.
  for (auto e : mesh.edges()) {
    if (ecm[e]) {
      auto h = mesh.halfedge(e);
      output.AddSegment<IK>(mesh.point(mesh.source(h)),
                            mesh.point(mesh.target(h)));
    }
  }

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

}  // namespace geometry