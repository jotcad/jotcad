#pragma once
#include "protocols.h"
#include <vector>

namespace jotcad {
namespace geo {

namespace math {

template <typename K>
static typename K::Point_3 catmull_rom(const typename K::Point_3& p0,
                                       const typename K::Point_3& p1,
                                       const typename K::Point_3& p2,
                                       const typename K::Point_3& p3,
                                       double t) {
    // t is from 0 to 1
    double t2 = t * t;
    double t3 = t2 * t;

    // Catmull-Rom coefficients
    double v0 = -0.5 * t3 + t2 - 0.5 * t;
    double v1 = 1.5 * t3 - 2.5 * t2 + 1.0;
    double v2 = -1.5 * t3 + 2.0 * t2 + 0.5 * t;
    double v3 = 0.5 * t3 - 0.5 * t2;

    // Use to_double for the calculation but return a Point_3 (which uses FT)
    // For extreme precision we could use FT for coefficients, but double t is usually fine for tessellation.
    double x = v0 * CGAL::to_double(p0.x()) + v1 * CGAL::to_double(p1.x()) +
               v2 * CGAL::to_double(p2.x()) + v3 * CGAL::to_double(p3.x());
    double y = v0 * CGAL::to_double(p0.y()) + v1 * CGAL::to_double(p1.y()) +
               v2 * CGAL::to_double(p2.y()) + v3 * CGAL::to_double(p3.y());
    double z = v0 * CGAL::to_double(p0.z()) + v1 * CGAL::to_double(p1.z()) +
               v2 * CGAL::to_double(p2.z()) + v3 * CGAL::to_double(p3.z());

    return typename K::Point_3(x, y, z);
}

} // namespace math

} // namespace geo
} // namespace jotcad
