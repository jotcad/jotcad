#pragma once

#include <CGAL/Ray_3.h>

#include "assets.h"
#include "geometry.h"
#include "shape.h"
#include "transform.h"

static GeometryId ArcSlice2(Assets& assets, Shape& shape, EK::FT start,
                            EK::FT end) {
  CGAL::Plane_3<EK> plane(0, 0, 1, 0);
  CGAL::Ray_3<EK> ray_3(CGAL::Point_3<EK>(0, 0, 0),
                        CGAL::Direction_3<EK>(1, 0, 0));
  CGAL::Ray_3<EK> start_3 = ray_3.transform(ComputeZTurnTf<EK>(start));
  CGAL::Ray_3<EK> end_3 = ray_3.transform(ComputeZTurnTf<EK>(end));
  Geometry geometry =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  std::optional<CGAL::Point_3<EK>> last;
  Geometry sliced;
  bool done = false;
  while (!done) {
    for (const auto& [s, t] : geometry.segments_) {
      CGAL::Segment_3<EK> s3(geometry.vertices_[s], geometry.vertices_[t]);
      if (last) {
        auto result = CGAL::intersection(end_3, s3);
        if (result) {
          if (const CGAL::Point_3<EK>* p =
                  std::get_if<CGAL::Point_3<EK>>(&*result)) {
            sliced.AddSegment(*last, *p);
            last.reset();
          } else if (const CGAL::Segment_3<EK>* s =
                         std::get_if<CGAL::Segment_3<EK>>(&*result)) {
            sliced.AddSegment(s->source(), s->target());
            last.reset();
          }
          done = true;
          break;
        } else {
          sliced.segments_.emplace_back(sliced.AddVertex(s3.source()),
                                        sliced.AddVertex(s3.target()));
          last = s3.target();
        }
      } else {
        auto result = CGAL::intersection(start_3, s3);
        if (result) {
          if (const CGAL::Point_3<EK>* p =
                  std::get_if<CGAL::Point_3<EK>>(&*result)) {
            sliced.AddSegment(*p, s3.target());
            last = s3.target();
          } else if (const CGAL::Segment_3<EK>* s =
                         std::get_if<CGAL::Segment_3<EK>>(&*result)) {
            sliced.AddSegment(s->source(), s->target());
            last = s->target();
          }
        }
      }
    }
  }
  return assets.TextId(sliced);
}
