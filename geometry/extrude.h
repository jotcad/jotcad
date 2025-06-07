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

static GeometryId Extrude2(Assets& assets, Shape& shape, Shape& top, Shape& bottom) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh;
  target.EncodeFaceSurfaceMesh<EK>(mesh);
  CGAL::Surface_mesh<CGAL::Point_3<EK>> extruded_mesh;
  if (!ExtrudeSurfaceMesh<EK>(mesh, top.GetTf(), bottom.GetTf(), extruded_mesh)) {
    return assets.UndefinedId();
  }
  Geometry output;
  output.DecodeSurfaceMesh<EK>(extruded_mesh);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

static GeometryId Extrude3(Assets& assets, Shape& shape, Shape& top, Shape& bottom) {
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh;
  target.EncodeSurfaceMesh<EK>(mesh);
  CGAL::Surface_mesh<CGAL::Point_3<EK>> extruded_mesh;
  if (!ExtrudeSurfaceMesh<EK>(mesh, top.GetTf(), bottom.GetTf(), extruded_mesh)) {
    return assets.UndefinedId();
  }
  Geometry output;
  output.DecodeSurfaceMesh<EK>(extruded_mesh);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}
