#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

#include <cmath>
#include <string>

namespace geometry {

template <typename K>
static void project(const typename K::Point_3& p, const std::string& mapping,
                    double& u, double& v, double& h) {
  double x = CGAL::to_double(p.x());
  double y = CGAL::to_double(p.y());
  double z = CGAL::to_double(p.z());

  if (mapping == "spherical") {
    h = std::sqrt(x * x + y * y + z * z);
    u = std::atan2(y, x);            // Longitude [-PI, PI]
    v = std::acos(z / (h + 1e-12));  // Latitude [0, PI]
  } else if (mapping == "cylindrical") {
    u = std::atan2(y, x);  // Angle [-PI, PI]
    v = z;                 // Height
    h = std::sqrt(x * x + y * y);
  } else {  // planar XY
    u = x;
    v = y;
    h = z;
  }
}

template <typename K>
static typename K::Point_3 unproject(double u, double v, double h,
                                     const std::string& mapping) {
  if (mapping == "spherical") {
    return typename K::Point_3(h * std::sin(v) * std::cos(u),
                               h * std::sin(v) * std::sin(u), h * std::cos(v));
  } else if (mapping == "cylindrical") {
    return typename K::Point_3(h * std::cos(u), h * std::sin(u), v);
  } else {
    return typename K::Point_3(u, v, h);
  }
}

}  // namespace geometry
