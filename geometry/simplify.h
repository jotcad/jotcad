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
#include "memory_util.h"
#include "repair_util.h"
#include "shape.h"
#include "simplify_util.h"

static GeometryId Simplify(Assets& assets, Shape& shape, int face_count) {
  Geometry source = assets.GetGeometry(shape.GeometryId());
  CGAL::Surface_mesh<CK::Point_3> mesh;
  source.EncodeSurfaceMesh<CK>(mesh);

  repair_stitching<CK>(mesh);
  size_t remaining_hole_count = repair_close_holes<CK>(mesh);
  std::vector<CK::Segment_3> segments;

  std::cout << "Memory/1: used=" << memory_used() << " free=" << memory_free()
            << std::endl;
  simplify<CK>(face_count, 90, mesh, segments);
  std::cout << "Memory/2: used=" << memory_used() << " free=" << memory_free()
            << std::endl;
  Geometry target;
  target.DecodeSurfaceMesh<CK>(mesh);
  return assets.TextId(target);
}
