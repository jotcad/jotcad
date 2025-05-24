#pragma once

#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "assets.h"
#include "geometry.h"
#include "repair_util.h"
#include "shape.h"

static bool Test(Assets& assets, Shape& shape, bool si) {
  std::cout << "QQ/Test" << std::endl;
  Geometry target =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
  CGAL::Surface_mesh<CGAL::Point_3<EK>> mesh;
  target.EncodeSurfaceMesh<EK>(mesh);

  bool failed = false;

  if (si) {
    size_t count = number_of_self_intersections(mesh);
    if (count > 0) {
      failed = true;
      std::cout << "Test/si: mesh=" << mesh << std::endl;
    }
    std::cout << "Test/si: count=" << count << std::endl;
  }

  return failed;
}
