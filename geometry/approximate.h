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
  std::cout << "Approximate/1" << std::endl;
  Geometry source = assets.GetGeometry(shape.GeometryId());
  CGAL::Surface_mesh<CK::Point_3> mesh;
  source.EncodeSurfaceMesh<CK>(mesh);

  repair_stitching<CK>(mesh);
  size_t remaining_hole_count = repair_close_holes<CK>(mesh);
  std::cout << "QQ/approximate: remaining_hole_count=" << remaining_hole_count
            << std::endl;

  std::vector<CK::Point_3> anchors;
  std::vector<std::array<std::size_t, 3>> triangles;
  MakeDeterministic();
  std::cout << "Approximate/input: face_count=" << face_count
            << " min_error_drop=" << min_error_drop
            << " number_of_faces=" << mesh.number_of_faces() << std::endl;
  if (!CGAL::Surface_mesh_approximation::approximate_triangle_mesh(
          mesh, CGAL::parameters::anchors(std::back_inserter(anchors))
                    .triangles(std::back_inserter(triangles))
                    .verbose_level(CGAL::Surface_mesh_approximation::VERBOSE)
                    //  .min_error_drop(min_error_drop)
                    //  .number_of_iterations(30)
                    .max_number_of_proxies(face_count))) {
    std::cout << "approximate_mesh failed to produce manifold output"
              << std::endl;
    // return assets.UndefinedId();
  }

  std::cout << "Approximate/7" << std::endl;

  /*
    std::vector<CK::Point_3> anchors;
    for (const auto& epick_anchor : epick_anchors) {
      anchors.emplace_back(epick_anchor.x(), epick_anchor.y(),
    epick_anchor.z());
    }
  */

  CGAL::Polygon_mesh_processing::repair_polygon_soup(anchors, triangles);
  CGAL::Polygon_mesh_processing::orient_polygon_soup(anchors, triangles);
  std::cout << "Approximate/8" << std::endl;

  CGAL::Surface_mesh<CK::Point_3> approximated_mesh;
  CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
      anchors, triangles, approximated_mesh);
  std::cout << "Approximate/9" << std::endl;

  std::cout << "Approximate/output: face_count=" << face_count
            << " min_error_drop=" << min_error_drop
            << " number_of_faces=" << mesh.number_of_faces() << std::endl;

  Geometry target;
  target.DecodeSurfaceMesh<CK>(approximated_mesh);

  return assets.TextId(target);
}
