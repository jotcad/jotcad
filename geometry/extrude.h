#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/transform.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

#include "assets.h"
#include "extrude_util.h"
#include "geometry.h"
#include "shape.h"

// TODO: Figure out if we really need to store faces.

static GeometryId Extrude2(Assets& assets, Shape& shape, Shape& top,
                           Shape& bottom) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

  Geometry output;
  const Tf& top_tf = top.GetTf();
  const Tf& bottom_tf = bottom.GetTf();

  // 1. Extrude faces
  CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh;
  target.EncodeFaceSurfaceMesh<EK>(mesh);
  if (!mesh.is_empty()) {
    CGAL::Surface_mesh<CGAL::Point_3<EK>> extruded_mesh;
    if (ExtrudeSurfaceMesh<EK>(mesh, top_tf, bottom_tf, extruded_mesh)) {
      output.DecodeSurfaceMesh<EK>(extruded_mesh);
    }
  }

  // 2. Extrude segments into quads
  for (const auto& [source, target_idx] : target.segments_) {
    const auto& s = target.vertices_[source];
    const auto& t = target.vertices_[target_idx];

    auto a = s.transform(bottom_tf);
    auto b = t.transform(bottom_tf);
    auto c = t.transform(top_tf);
    auto d = s.transform(top_tf);

    std::vector<size_t> face;
    face.push_back(output.AddVertex(a));
    face.push_back(output.AddVertex(b));
    face.push_back(output.AddVertex(c));
    face.push_back(output.AddVertex(d));
    output.faces_.emplace_back(std::move(face),
                               std::vector<std::vector<size_t>>());
  }

  // 3. Extrude points into segments
  for (const auto& point_idx : target.points_) {
    const auto& p = target.vertices_[point_idx];
    auto a = p.transform(bottom_tf);
    auto b = p.transform(top_tf);
    output.AddSegment(a, b);
  }

  if (output.vertices_.empty()) {
    return assets.UndefinedId();
  }

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

static GeometryId Extrude3(Assets& assets, Shape& shape, Shape& top,
                           Shape& bottom) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

  Geometry output;
  const Tf& top_tf = top.GetTf();
  const Tf& bottom_tf = bottom.GetTf();

  // 1. Extrude surface mesh
  CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh;
  target.EncodeSurfaceMesh<EK>(mesh);
  if (!mesh.is_empty()) {
    CGAL::Surface_mesh<CGAL::Point_3<EK>> extruded_mesh;
    if (ExtrudeSurfaceMesh<EK>(mesh, top_tf, bottom_tf, extruded_mesh)) {
      output.DecodeSurfaceMesh<EK>(extruded_mesh);
    }
  }

  // 2. Extrude segments into quads
  for (const auto& [source, target_idx] : target.segments_) {
    const auto& s = target.vertices_[source];
    const auto& t = target.vertices_[target_idx];

    auto a = s.transform(bottom_tf);
    auto b = t.transform(bottom_tf);
    auto c = t.transform(top_tf);
    auto d = s.transform(top_tf);

    std::vector<size_t> face;
    face.push_back(output.AddVertex(a));
    face.push_back(output.AddVertex(b));
    face.push_back(output.AddVertex(c));
    face.push_back(output.AddVertex(d));
    output.faces_.emplace_back(std::move(face),
                               std::vector<std::vector<size_t>>());
  }

  // 3. Extrude points into segments
  for (const auto& point_idx : target.points_) {
    const auto& p = target.vertices_[point_idx];
    auto a = p.transform(bottom_tf);
    auto b = p.transform(top_tf);
    output.AddSegment(a, b);
  }

  if (output.vertices_.empty()) {
    return assets.UndefinedId();
  }

  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}