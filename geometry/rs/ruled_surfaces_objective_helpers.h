#pragma once

#include "ruled_surfaces_base.h"

namespace ruled_surfaces {
namespace internal {

inline double dihedral_angle_cosine(const PointCgal& p1, const PointCgal& p2,
                                    const PointCgal& p3, const PointCgal& p4) {
  VectorCgal n1 = CGAL::cross_product(p2 - p1, p3 - p1);
  VectorCgal n2 = CGAL::cross_product(p4 - p1, p2 - p1);
  double n1_sq_len = n1.squared_length();
  double n2_sq_len = n2.squared_length();
  if (n1_sq_len < kEpsilon * kEpsilon || n2_sq_len < kEpsilon * kEpsilon) {
    return 1.0;
  }
  double cos_val = (n1 * n2) / std::sqrt(n1_sq_len * n2_sq_len);
  return cos_val < -1.0 ? -1.0 : (cos_val > 1.0 ? 1.0 : cos_val);
}

}  // namespace internal
}  // namespace ruled_surfaces
