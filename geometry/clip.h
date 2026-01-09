#pragma once

#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

static GeometryId Clip(Assets& assets, Shape& shape,
                       std::vector<Shape>& tool_shapes) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> target_mesh;
  target.EncodeSurfaceMesh<EK>(target_mesh);

  CGAL::Surface_mesh<CGAL::Point_3<EK>> composed_tool;
  for (Shape& tool_shape : tool_shapes) {
    tool_shape.Walk([&](Shape& tool_sub_shape) {
      if (tool_sub_shape.GetBooleanTag("isPlane")) {
        EK::Plane_3 plane(
            tool_sub_shape.GetTf().transform(EK::Point_3(0, 0, 0)),
            tool_sub_shape.GetTf().transform(EK::Vector_3(0, 0, 1)));
        bool is_closed = CGAL::is_closed(target_mesh);
        // Clipping keeps the negative side.
        CGAL::Polygon_mesh_processing::clip(
            target_mesh, plane,
            CGAL::parameters::clip_volume(is_closed).use_compact_clipper(true));

        if (!is_closed) {
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
        }
        return true;
      }
      if (!tool_sub_shape.GetBooleanTag("isGap") &&
          tool_sub_shape.HasGeometryId()) {
        Geometry tool = assets.GetGeometry(tool_sub_shape.GeometryId())
                            .Transform(tool_sub_shape.GetTf());
        CGAL::Surface_mesh<CGAL::Point_3<EK>> tool_mesh;
        tool.EncodeSurfaceMesh<EK>(tool_mesh);
        if (CGAL::is_empty(composed_tool)) {
          composed_tool = tool_mesh;
        } else if (!CGAL::Polygon_mesh_processing::corefine_and_compute_union(
                       composed_tool, tool_mesh, composed_tool,
                       CGAL::parameters::throw_on_self_intersection(true),
                       CGAL::parameters::all_default(),
                       CGAL::parameters::all_default())) {
          std::cout << "Clip: non-manifold" << std::endl;
        }
      }
      return true;
    });
  }

  if (!CGAL::is_empty(composed_tool)) {
    if (!CGAL::Polygon_mesh_processing::corefine_and_compute_intersection(
            target_mesh, composed_tool, target_mesh,
            CGAL::parameters::throw_on_self_intersection(true),
            CGAL::parameters::all_default(), CGAL::parameters::all_default())) {
      std::cout << "Clip: non-manifold" << std::endl;
    }
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
