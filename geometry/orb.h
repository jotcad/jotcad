#pragma once

// TODO: This interface needs to be redefined to use Assets rather than Geometry *.
// The Assets object should be passed to allow for proper asset management (e.g., caching).

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/IO/facets_in_complex_2_to_triangle_mesh.h>
#include <CGAL/Implicit_surface_3.h>
#include <CGAL/Mesh_criteria_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Surface_mesh_default_triangulation_3.h> // Added
#include <CGAL/Complex_2_in_triangulation_3.h>        // Added

#include "assets.h" // Include Assets.h
#include "geometry.h"

template <typename FT, typename Point>
static FT unitSphereFunction(Point p) {
  const FT x2 = p.x() * p.x(), y2 = p.y() * p.y(), z2 = p.z() * p.z();
  return x2 + y2 + z2 - 1;
}

static GeometryId MakeOrb(Assets& assets, double angular_bound,
                          double radius_bound, double distance_bound) {
  typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;
  typedef CGAL::Surface_mesh_default_triangulation_3 Tr;
  typedef CGAL::Complex_2_in_triangulation_3<Tr> C2t3;
  typedef Tr::Geom_traits GT;
  typedef GT::Sphere_3 Sphere_3;
  typedef GT::Point_3 Point_3;
  typedef GT::FT FT;
  typedef FT (*Function)(Point_3);
  typedef CGAL::Implicit_surface_3<GT, Function> Surface_3;
  Tr tr;          // 3D-Delaunay triangulation
  C2t3 c2t3(tr);  // 2D-complex in 3D-Delaunay triangulation

  CGAL::get_default_random() = CGAL::Random(0);
  std::srand(0);

  Surface_3 surface(unitSphereFunction<FT, Point_3>, Sphere_3(CGAL::ORIGIN, 2));
  CGAL::Surface_mesh_default_criteria_3<Tr> criteria(
      angular_bound, radius_bound, distance_bound);
  // meshing surface
  CGAL::make_surface_mesh(c2t3, surface, criteria, CGAL::Manifold_tag());
  CGAL::Surface_mesh<IK::Point_3> epick_mesh;
  CGAL::facets_in_complex_2_to_triangle_mesh(c2t3, epick_mesh);

  Geometry geometry; // Create a local Geometry object
  geometry.DecodeSurfaceMesh<IK>(epick_mesh);
  return assets.TextId(geometry); // Return GeometryId via assets
}