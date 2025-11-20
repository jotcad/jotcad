#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Cut(Assets& assets, Shape& shape,
                      std::vector<Shape>& tool_shapes) {
  std::cout << "--- Cut function entry ---" << std::endl;
  std::cout << "Target Shape TF: " << shape.GetTf().ToString() << std::endl;

  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh<EK>(target_mesh);
  std::cout << "Initial target_mesh: Vertices=" << target_mesh.number_of_vertices()
            << ", Faces=" << target_mesh.number_of_faces() << std::endl;

  std::function<bool(Shape&)> walk = [&](Shape& tool_shape) {
    std::cout << "--- Walk function for tool_shape ---" << std::endl;
    std::cout << "Tool Shape TF: " << tool_shape.GetTf().ToString() << std::endl;
    Shape mask_shape;
    if (tool_shape.GetMask(mask_shape)) {
      std::cout << "  Tool has mask, walking mask_shape." << std::endl;
      walk(mask_shape);
    } else if (tool_shape.HasGeometryId()) {
      Geometry tool =
          assets.GetGeometry(tool_shape.GeometryId()).Transform(tool_shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
      tool.EncodeSurfaceMesh<EK>(tool_mesh);
      std::cout << "  tool_mesh: Vertices=" << tool_mesh.number_of_vertices()
                << ", Faces=" << tool_mesh.number_of_faces() << std::endl;

      std::cout << "  target_mesh before difference: Vertices=" << target_mesh.number_of_vertices()
                << ", Faces=" << target_mesh.number_of_faces() << std::endl;
      std::cout << "  tool_mesh before difference: Vertices=" << tool_mesh.number_of_vertices()
                << ", Faces=" << tool_mesh.number_of_faces() << std::endl;
      std::cout << "  target_mesh before difference: Vertices=" << target_mesh.number_of_vertices()
                << ", Faces=" << target_mesh.number_of_faces() << std::endl;
      std::cout << "  tool_mesh before difference: Vertices=" << tool_mesh.number_of_vertices()
                << ", Faces=" << tool_mesh.number_of_faces() << std::endl;
      bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
              target_mesh, tool_mesh, target_mesh,
              CGAL::parameters::throw_on_self_intersection(true),
              CGAL::parameters::all_default(),
              CGAL::parameters::all_default());
      std::cout << "  corefine_and_compute_difference returned: " << (success ? "true" : "false") << std::endl;

      if (!success) {
        Napi::Env env = assets.Env();
        Napi::Error::New(env, "Cut: non-manifold").ThrowAsJavaScriptException();
      }
      std::cout << "  target_mesh after difference: Vertices=" << target_mesh.number_of_vertices()
                << ", Faces=" << target_mesh.number_of_faces() << std::endl;
      tool_shape.ForShapes(walk);
    }
    return true;
  };

  for (Shape& tool_shape : tool_shapes) {
    walk(tool_shape);
  }

  // TODO: Preserve the non-triangle geometry.
  CGAL::Surface_mesh<CGAL::Point_3<EK>> output_mesh;
  CGAL::Polygon_mesh_processing::remesh_planar_patches(target_mesh,
                                                       output_mesh);
  CGAL::Polygon_mesh_processing::triangulate_faces(output_mesh);
  Geometry output;
  output.DecodeSurfaceMesh<EK>(output_mesh);
  std::cout << "Final output_mesh: Vertices=" << output_mesh.number_of_vertices()
            << ", Faces=" << output_mesh.number_of_faces() << std::endl;

  std::cout << "--- Cut function exit ---" << std::endl;
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}
