#pragma once

#include "ruled_surfaces_objective_helpers.h"
#include "ruled_surfaces_base.h"

namespace ruled_surfaces {
namespace internal {
// Cost for MIN_MEAN_CURVATURE_VARIATION
// p1-p2 is shared edge. t1=(p1,p2,p3), t2=(p2,p1,p4)
inline double mean_curvature_variation_cost(const PointCgal& p1,
                                            const PointCgal& p2,
                                            const PointCgal& p3,
                                            const PointCgal& p4) {
  double area1 = triangle_area(p1, p2, p3);
  double area2 = triangle_area(p2, p1, p4);
  if (area1 < kEpsilon || area2 < kEpsilon) return 0;
  double cos_d =
      std::max(-1.0, std::min(1.0, dihedral_angle_cosine(p1, p2, p3, p4)));
  double angle = acos(cos_d);
  double edge_len_sq = (p2 - p1).squared_length();
  if (edge_len_sq < kEpsilon * kEpsilon) return 0;
  return angle * angle * edge_len_sq / (area1 + area2);
}
}  // namespace internal

struct MinMeanCurvatureVariation : public Objective {
  double calculate_cost(const Mesh& mesh) const override;
  bool is_dlg_compatible() const override { return true; }
  double get_dihedral_cost(const PointCgal& p1, const PointCgal& p2,
                           const PointCgal& p3,
                           const PointCgal& p4) const override;
};

inline double MinMeanCurvatureVariation::get_dihedral_cost(
    const PointCgal& p1, const PointCgal& p2, const PointCgal& p3,
    const PointCgal& p4) const {
  return internal::mean_curvature_variation_cost(p1, p2, p3, p4);
}

inline double MinMeanCurvatureVariation::calculate_cost(
    const Mesh& mesh) const {
  double total_cost = 0.0;
  for (auto edge_id : mesh.edges()) {
    auto hf = mesh.halfedge(edge_id, 0);
    if (mesh.is_border(hf) || mesh.is_border(mesh.opposite(hf))) continue;
    PointCgal p1 = mesh.point(mesh.source(hf));
    PointCgal p2 = mesh.point(mesh.target(hf));
    PointCgal p3 = mesh.point(mesh.target(mesh.next(hf)));
    PointCgal p4 = mesh.point(mesh.target(mesh.next(mesh.opposite(hf))));
    total_cost += internal::mean_curvature_variation_cost(p1, p2, p3, p4);
  }
  return total_cost;
}

} // namespace ruled_surfaces
