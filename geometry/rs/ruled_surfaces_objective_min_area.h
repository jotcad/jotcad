#pragma once

#include "ruled_surfaces_base.h"

namespace ruled_surfaces {

struct MinArea : public Objective {
  double operator()(const PointCgal& p1, const PointCgal& p2,
                    const PointCgal& p3) const {
    return internal::triangle_area(p1, p2, p3);
  }
  double calculate_cost(const Mesh& mesh) const override;
  bool is_slg_compatible() const override { return true; }
  bool is_dlg_compatible() const override { return true; }
  double get_dihedral_cost(const PointCgal& p1, const PointCgal& p2,
                           const PointCgal& p3,
                           const PointCgal& p4) const override;
};

inline double MinArea::get_dihedral_cost(const PointCgal& p1,
                                          const PointCgal& p2,
                                          const PointCgal&,
                                          const PointCgal& p4) const {
  return internal::triangle_area(p1, p2, p4);
}

inline double MinArea::calculate_cost(const Mesh& mesh) const {
  double total_cost = 0.0;
  for (auto face_id : mesh.faces()) {
    auto hf = mesh.halfedge(face_id);
    PointCgal p0 = mesh.point(mesh.source(hf));
    PointCgal p1 = mesh.point(mesh.target(hf));
    PointCgal p2 = mesh.point(mesh.target(mesh.next(hf)));
    total_cost += internal::triangle_area(p0, p1, p2);
  }
  return total_cost;
}

} // namespace ruled_surfaces
