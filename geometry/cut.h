#pragma once

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Cut(Assets& assets, Shape& shape,
                      std::vector<Shape>& tool_shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh<EK>(target_mesh);

  std::function<bool(Shape&)> walk = [&](Shape& tool_shape) {
    Shape mask_shape;
    if (tool_shape.GetMask(mask_shape)) {
      walk(mask_shape);
    } else if (tool_shape.GetBooleanTag("isPlane")) {
      EK::Point_3 plane_point =
          tool_shape.GetTf().transform(EK::Point_3(0, 0, 0));
      EK::Vector_3 plane_normal =
          tool_shape.GetTf().transform(EK::Vector_3(0, 0, 1));
      EK::Plane_3 plane(plane_point, plane_normal);

      // Perform the open cut first.
      CGAL::Polygon_mesh_processing::clip(
          target_mesh, plane.opposite(),
          CGAL::parameters::clip_volume(false).use_compact_clipper(true));

      // Now identify boundary cycles that are on the plane and close them.
      std::vector<typename CGAL::Surface_mesh<EK::Point_3>::Halfedge_index>
          border_cycles;
      CGAL::Polygon_mesh_processing::extract_boundary_cycles(
          target_mesh, std::back_inserter(border_cycles));

      for (auto h : border_cycles) {
        bool all_on_plane = true;
        for (auto v : target_mesh.vertices_around_face(h)) {
          if (plane.has_on(target_mesh.point(v))) {
            continue;
          }
          // Use a small epsilon for robustness if needed, but EK should be
          // exact.
          if (CGAL::squared_distance(plane, target_mesh.point(v)) > 0) {
            all_on_plane = false;
            break;
          }
        }

        if (all_on_plane) {
          std::vector<typename CGAL::Surface_mesh<EK::Point_3>::Face_index>
              patch_facets;
          CGAL::Polygon_mesh_processing::triangulate_hole(
              target_mesh, h,
              CGAL::parameters::face_output_iterator(
                  std::back_inserter(patch_facets)));
        }
      }
      tool_shape.ForShapes(walk);
    } else if (tool_shape.HasGeometryId()) {
      Geometry tool = assets.GetGeometry(tool_shape.GeometryId())
                          .Transform(tool_shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
      tool.EncodeSurfaceMesh<EK>(tool_mesh);
      bool success =
          CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
              target_mesh, tool_mesh, target_mesh,
              CGAL::parameters::throw_on_self_intersection(true),
              CGAL::parameters::all_default(), CGAL::parameters::all_default());
      if (!success) {
        Napi::Env env = assets.Env();
        Napi::Error::New(env, "Cut: non-manifold").ThrowAsJavaScriptException();
      }
      tool_shape.ForShapes(walk);
    } else {
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
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}