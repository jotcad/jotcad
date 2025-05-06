#pragma once

#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

#include "assets.h"
#include "shape.h"
#include "geometry.h"

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;

static GeometryId Triangulate(Assets& assets, GeometryId id) {
  CGAL::Surface_mesh<EK::Point_3> mesh = assets.GetSurfaceMesh(id);
  Geometry triangulated(mesh);
  return assets.TextId(triangulated);
}
