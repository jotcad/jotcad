#pragma once

#include <CGAL/Polygon_mesh_processing/internal/Snapping/snap.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/polygon_mesh_to_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Surface_mesh_approximation/approximate_triangle_mesh.h>
#include <CGAL/boost/graph/helpers.h>

#include "assets.h"
#include "random_util.h"
#include "repair_util.h"
#include "shape.h"

static GeometryId Approximate(Assets& assets, Shape& shape, int face_count,
                              double min_error_drop = 0.01) {
  Geometry source = assets.GetGeometry(shape.GeometryId());
  CGAL::Surface_mesh<CK::Point_3> mesh;
  source.EncodeSurfaceMesh<CK>(mesh);

  // repair_stitching<CK>(mesh);
  // size_t remaining_hole_count = repair_close_holes<CK>(mesh);

  repair_stitching<CK>(mesh);
  repair_close_holes<CK>(mesh);
  repair_degeneracies<CK>(mesh);

  std::vector<CK::Point_3> anchors;
  std::vector<std::array<std::size_t, 3>> triangles;
  MakeDeterministic();
  std::cout << "Approximate: face_count=" << face_count
            << " min_error_drop=" << min_error_drop
            << " number_of_faces=" << mesh.number_of_faces() << std::endl;
  if (!CGAL::Surface_mesh_approximation::approximate_triangle_mesh(
          mesh, CGAL::parameters::anchors(std::back_inserter(anchors))
                    .triangles(std::back_inserter(triangles))
                    .verbose_level(CGAL::Surface_mesh_approximation::VERBOSE)
                    //  .min_error_drop(min_error_drop)
                    //  .number_of_iterations(30)
                    .max_number_of_proxies(face_count))) {
    std::cout << "Approximate: failed to produce manifold output"
              << std::endl;
    return assets.UndefinedId();
  }

  CGAL::Polygon_mesh_processing::repair_polygon_soup(anchors, triangles);
  CGAL::Polygon_mesh_processing::orient_polygon_soup(anchors, triangles);

  CGAL::Surface_mesh<CK::Point_3> approximated_mesh;
  CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
      anchors, triangles, approximated_mesh);

  std::cout << "Approximate: face_count=" << face_count
            << " min_error_drop=" << min_error_drop
            << " number_of_faces=" << mesh.number_of_faces() << std::endl;

  Geometry target;
  target.DecodeSurfaceMesh<CK>(approximated_mesh);

  return assets.TextId(target);
}
