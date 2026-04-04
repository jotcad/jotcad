#pragma once
#include "hash.h"

static std::string BuildPreciseSegmentsGeometry(
    Geometries& geometries, const std::vector<CGAL::Segment_3<EK>>& segments) {
  Napi::Object jsSegments = Napi::Object::New(geometries.Env());
  jsSegments.Set("type", "precise_segments");
  std::ostringstream s;
  s << std::setiosflags(std::ios_base::fixed) << std::setprecision(0);
  for (const auto& segment : segments) {
    s << segment << ", ";
  }
  jsSegments.Set("precise_segments", s.str());
  auto id = ComputeGeometryHash(jsSegments);
  geometries.Set(id, jsSegments);
  return id;
}
