#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "repair_util.h"
#include "shape.h"

namespace PMP = CGAL::Polygon_mesh_processing;

#include <cassert>

#include "robust_boolean.h"

static GeometryId Join(Assets& assets, Shape& shape, std::vector<Shape>& shapes,
                       double envelope_size = 0.01) {
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;

  std::function<void(Shape&)> join_walk = [&](Shape& s) {
    if (!s.GetBooleanTag("isGap") && s.HasGeometryId()) {
      Geometry tool = assets.GetGeometry(s.GeometryId()).Transform(s.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> source_mesh;
      tool.EncodeSurfaceMesh<EK>(source_mesh);

      if (!robust_union<EK>(target_mesh, source_mesh, envelope_size)) {
        Napi::Error::New(assets.Env(),
                         "Join: RobustUnion failed to produce a valid result.")
            .ThrowAsJavaScriptException();
      }
    }
    s.ForShapes([&](Shape& child) {
      join_walk(child);
      return true;
    });
  };

  // Include the base shape in the join.
  join_walk(shape);

  // Include all additional tool shapes.
  for (size_t i = 0; i < shapes.size(); ++i) {
    join_walk(shapes[i]);
  }

  std::string manifold_report = "";
  if (!CGAL::is_closed(target_mesh)) {
    manifold_report += "Mesh is not closed. ";
  }
  if (!target_mesh.is_valid(
          true)) {  // Checks if the mesh is valid and manifold
    manifold_report += "Mesh is not valid or not manifold. ";
  }

  if (!manifold_report.empty()) {
    std::cout << "Join: Throwing JavaScript exception: Non-manifold geometry "
                 "detected - "
              << manifold_report << std::endl;
    Napi::Error::New(assets.Env(),
                     std::string("Join: Non-manifold geometry detected - ") +
                         manifold_report)
        .ThrowAsJavaScriptException();
  }

  CGAL::Surface_mesh<CGAL::Point_3<EK>> output_mesh;
  PMP::remesh_planar_patches(target_mesh, output_mesh);
  PMP::triangulate_faces(output_mesh);
  Geometry output;
  output.DecodeSurfaceMesh<EK>(output_mesh);

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}