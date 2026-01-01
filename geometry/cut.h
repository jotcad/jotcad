#pragma once

#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"

typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

static void GeometryToPolygonsWithHoles(
    const Geometry& geometry, std::vector<Polygon_with_holes_2>& pwhs) {
  for (const auto& [face, holes] : geometry.faces_) {
    CGAL::Polygon_2<EK> boundary;
    for (size_t vIdx : face) {
      boundary.push_back(CGAL::Point_2<EK>(geometry.vertices_[vIdx].x(),
                                           geometry.vertices_[vIdx].y()));
    }
    if (boundary.is_empty()) continue;
    if (boundary.orientation() != CGAL::Sign::POSITIVE)
      boundary.reverse_orientation();

    std::vector<CGAL::Polygon_2<EK>> hole_polygons;
    for (const auto& hole : holes) {
      CGAL::Polygon_2<EK> h;
      for (size_t vIdx : hole) {
        h.push_back(CGAL::Point_2<EK>(geometry.vertices_[vIdx].x(),
                                      geometry.vertices_[vIdx].y()));
      }
      if (h.is_empty()) continue;
      if (h.orientation() != CGAL::Sign::NEGATIVE) h.reverse_orientation();
      hole_polygons.push_back(std::move(h));
    }
    pwhs.emplace_back(boundary, hole_polygons.begin(), hole_polygons.end());
  }
}

static void ShapeToGPS(Assets& assets, Shape& shape,
                       General_polygon_set_2& gps) {
  shape.Walk([&](Shape& sub) {
    if (sub.HasGeometryId()) {
      Geometry g = assets.GetGeometry(sub.GeometryId()).Transform(sub.GetTf());
      std::vector<Polygon_with_holes_2> pwhs;
      GeometryToPolygonsWithHoles(g, pwhs);
      for (const auto& pwh : pwhs) {
        gps.join(pwh);
      }
    }
    return true;
  });
}

static Geometry GPSToGeometry(const General_polygon_set_2& gps) {
  Geometry g;
  std::vector<Polygon_with_holes_2> pwhs;
  gps.polygons_with_holes(std::back_inserter(pwhs));
  for (const auto& pwh : pwhs) {
    std::vector<size_t> face;
    for (const auto& p : pwh.outer_boundary()) {
      face.push_back(g.AddVertex(EK::Point_3(p.x(), p.y(), 0)));
    }
    std::vector<std::vector<size_t>> holes;
    for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
      std::vector<size_t> hole;
      for (const auto& p : *hit) {
        hole.push_back(g.AddVertex(EK::Point_3(p.x(), p.y(), 0)));
      }
      holes.push_back(std::move(hole));
    }
    g.faces_.emplace_back(face, holes);
  }
  return g;
}

static GeometryId Cut2(Assets& assets, Shape& shape,
                       std::vector<Shape>& tool_shapes) {
  General_polygon_set_2 target_set;
  ShapeToGPS(assets, shape, target_set);

  for (Shape& tool_shape : tool_shapes) {
    General_polygon_set_2 tool_set;
    ShapeToGPS(assets, tool_shape, tool_set);
    target_set.difference(tool_set);
  }

  Geometry output = GPSToGeometry(target_set);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

static GeometryId Cut3(Assets& assets, Shape& shape,
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
        Napi::Error::New(env, "Cut3: non-manifold")
            .ThrowAsJavaScriptException();
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

  CGAL::Surface_mesh<CGAL::Point_3<EK>> output_mesh;
  CGAL::Polygon_mesh_processing::remesh_planar_patches(target_mesh,
                                                       output_mesh);
  CGAL::Polygon_mesh_processing::triangulate_faces(output_mesh);
  Geometry output;
  output.DecodeSurfaceMesh<EK>(output_mesh);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}
