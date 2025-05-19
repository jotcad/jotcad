#pragma once

#include <CGAL/Arr_extended_dcel.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arrangement_2.h>

#include "arrangement.h"
#include "assets.h"
#include "geometry.h"
#include "shape.h"

// We limit fill to the Z0 plane.

static GeometryId Fill(Assets& assets, std::vector<Shape>& shapes, bool holes) {
  typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
  typedef CGAL::Arr_segment_traits_2<EK> Traits_2;
  typedef CGAL::Arr_extended_dcel<Traits_2, size_t, size_t, size_t>
      Dcel_with_regions;
  typedef CGAL::Arrangement_2<Traits_2, Dcel_with_regions>
      Arrangement_with_regions_2;

  Arrangement_with_regions_2 arrangement;

  CGAL::Plane_3<EK> plane(0, 0, 1, 0);

  for (Shape& shape : shapes) {
    const auto id = shape.GeometryId();
    Tf tf = shape.GetTf();
    Geometry geometry =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    for (const auto& [source, target] : geometry.segments_) {
      CGAL::Segment_3<EK> s3(geometry.vertices_[source],
                             geometry.vertices_[target]);
      if (s3.source() == s3.target()) {
        continue;
      }
      CGAL::Segment_2<EK> s2(plane.to_2d(s3.source()),
                             plane.to_2d(s3.target()));
      if (s2.source() == s2.target()) {
        continue;
      }
      insert(arrangement, s2);
    }
  }

  // Convert the arrangements to polygons.

  std::vector<CGAL::Polygon_with_holes_2<EK>> pwhs;
  if (holes) {
    convertArrangementToPolygonsWithHolesEvenOdd(arrangement, pwhs);
  } else {
    convertArrangementToPolygonsWithHolesNonZero(arrangement, pwhs);
  }

  // Convert the polygons to faces.
  Geometry filled;
  for (const auto& pwh : pwhs) {
    filled.faces_.emplace_back();
    auto& face = filled.faces_.back().first;
    auto& holes = filled.faces_.back().second;
    for (const auto& point : pwh.outer_boundary()) {
      face.push_back(filled.AddVertex(plane.to_3d(point), true));
    }
    for (const auto& pwh_hole : pwh.holes()) {
      holes.emplace_back();
      auto& hole = holes.back();
      for (const auto& point : pwh_hole) {
        hole.push_back(filled.AddVertex(plane.to_3d(point), true));
      }
    }
  }
  return assets.TextId(filled);
}
