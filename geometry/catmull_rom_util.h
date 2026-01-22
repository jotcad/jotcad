#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>

namespace geometry {

template <typename K>
static typename K::Point_3 catmull_rom(const typename K::Point_3& p0,
                                       const typename K::Point_3& p1,
                                       const typename K::Point_3& p2,
                                       const typename K::Point_3& p3,
                                       double t) {
  double t2 = t * t;
  double t3 = t2 * t;

  double v0 = -0.5 * t3 + t2 - 0.5 * t;
  double v1 = 1.5 * t3 - 2.5 * t2 + 1.0;
  double v2 = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
  double v3 = 0.5 * t3 - 0.5 * t2;

  double x = v0 * CGAL::to_double(p0.x()) + v1 * CGAL::to_double(p1.x()) +
             v2 * CGAL::to_double(p2.x()) + v3 * CGAL::to_double(p3.x());
  double y = v0 * CGAL::to_double(p0.y()) + v1 * CGAL::to_double(p1.y()) +
             v2 * CGAL::to_double(p2.y()) + v3 * CGAL::to_double(p3.y());
  double z = v0 * CGAL::to_double(p0.z()) + v1 * CGAL::to_double(p1.z()) +
             v2 * CGAL::to_double(p2.z()) + v3 * CGAL::to_double(p3.z());

  return typename K::Point_3(x, y, z);
}

}  // namespace geometry
