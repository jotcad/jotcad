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

static GeometryId Join(Assets& assets, Shape& shape,
                       std::vector<Shape>& shapes) {
  std::cerr << "DEBUG: Join entered." << std::endl;

  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  std::cerr << "DEBUG: Join: Starting loop for remaining shapes." << std::endl;
  for (size_t i = 0; i < shapes.size(); ++i) {
    std::cerr << "DEBUG: Join: Processing shape element " << i << std::endl;
    shapes[i].Walk([&](Shape& sub_shape) {
      if (!sub_shape.HasGeometryId()) {
        return true;
      }
      Geometry tool = assets.GetGeometry(sub_shape.GeometryId())
                          .Transform(sub_shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> source_mesh;
      tool.EncodeSurfaceMesh<EK>(source_mesh);

      std::cerr << "DEBUG: Join: Calling robust_union for shape element " << i
                << std::endl;
      if (!robust_union<EK>(target_mesh, source_mesh)) {
        std::cerr << "ERROR: Join: RobustUnion failed for shape element " << i
                  << std::endl;
        Napi::Error::New(assets.Env(),
                         "Join: RobustUnion failed to produce a valid result.")
            .ThrowAsJavaScriptException();
      }
      return true;
    });
  }
  std::cerr
      << "DEBUG: Join: Loop for remaining shapes finished. Preparing output."
      << std::endl;

  std::string manifold_report = "";
  if (!CGAL::is_closed(target_mesh)) {
    manifold_report += "Mesh is not closed. ";
  }
  if (!target_mesh.is_valid(
          true)) {  // Checks if the mesh is valid and manifold
    manifold_report += "Mesh is not valid or not manifold. ";
  }
  // TODO: Add more specific checks if needed, e.g., for vertex/edge
  // manifoldness PMP::is_vertex_manifold(target_mesh)
  // PMP::is_edge_manifold(target_mesh)

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
