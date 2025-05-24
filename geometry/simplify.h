#pragma once

#include <CGAL/Polygon_mesh_processing/internal/Snapping/snap.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/polygon_mesh_to_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Simple_cartesian.h>
#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
#include <CGAL/boost/graph/helpers.h>

#include "assets.h"
#include "repair_util.h"
#include "shape.h"
#include "simplify_util.h"

static GeometryId Simplify(Assets& assets, Shape& shape, int face_count) {
  std::cout << "Simplify/1 id="
            << shape.GeometryId().As<Napi::String>().Utf8Value() << std::endl;
  Geometry source = assets.GetGeometry(shape.GeometryId());
  std::cout << "Simplify/2" << std::endl;
  CGAL::Surface_mesh<CK::Point_3> mesh;
  std::cout << "Simplify/3" << std::endl;
  source.EncodeSurfaceMesh<CK>(mesh);
  repair_stitching<CK>(mesh);
  repair_close_holes<CK>(mesh);
  repair_degeneracies<CK>(mesh);
  simplify<CK>(face_count, mesh);
  Geometry target;
  target.DecodeSurfaceMesh<CK>(mesh);
  return assets.TextId(target);
}
