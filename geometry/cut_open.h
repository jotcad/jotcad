#pragma once

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId CutOpen(Assets& assets, Shape& shape,
                          std::vector<Shape>& tool_shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh<EK>(target_mesh);

  for (Shape& tool_shape : tool_shapes) {
    tool_shape.Walk([&](Shape& tool_sub_shape) {
      if (tool_sub_shape.GetBooleanTag("isPlane")) {
        EK::Plane_3 plane(
            tool_sub_shape.GetTf().transform(EK::Point_3(0, 0, 0)),
            tool_sub_shape.GetTf().transform(EK::Vector_3(0, 0, 1)));
        CGAL::Polygon_mesh_processing::clip(
            target_mesh, plane.opposite(),
            CGAL::parameters::clip_volume(false).use_compact_clipper(true));
        return true;
      }
      if (!tool_sub_shape.HasGeometryId()) {
        return true;
      }
      Geometry tool = assets.GetGeometry(tool_sub_shape.GeometryId())
                          .Transform(tool_sub_shape.GetTf());
      CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
      tool.EncodeSurfaceMesh<EK>(tool_mesh);

      // Reverse orientation so that clip removes the interior of the tool.
      CGAL::Polygon_mesh_processing::reverse_face_orientations(tool_mesh);

      // We use clip to cut holes. clip_volume(false) ensures we don't close the
      // result.
      CGAL::Polygon_mesh_processing::clip(
          target_mesh, tool_mesh,
          CGAL::parameters::clip_volume(false).use_compact_clipper(true));

      return true;
    });
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