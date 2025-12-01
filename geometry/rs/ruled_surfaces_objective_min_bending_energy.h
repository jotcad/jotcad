#pragma once

#include "ruled_surfaces_objective_helpers.h"

namespace ruled_surfaces {

struct MinBendingEnergy : public Objective {
  double calculate_cost(const Mesh& mesh) const override;
  bool is_dlg_compatible() const override { return true; }
  double get_dihedral_cost(const PointCgal& p1, const PointCgal& p2,
                           const PointCgal& p3,
                           const PointCgal& p4) const override;
};

inline double MinBendingEnergy::get_dihedral_cost(const PointCgal& p1,
                                                  const PointCgal& p2,
                                                  const PointCgal& p3,
                                                  const PointCgal& p4) const {
  return 1.0 - internal::dihedral_angle_cosine(p1, p2, p3, p4);
}

inline double MinBendingEnergy::calculate_cost(const Mesh& mesh) const {
  double total_cost = 0.0;
  for (auto edge_id : mesh.edges()) {
    auto hf = mesh.halfedge(edge_id, 0);
    if (mesh.is_border(hf) || mesh.is_border(mesh.opposite(hf))) continue;
    PointCgal p1 = mesh.point(mesh.source(hf));
    PointCgal p2 = mesh.point(mesh.target(hf));
    PointCgal p3 = mesh.point(mesh.target(mesh.next(hf)));
    PointCgal p4 = mesh.point(mesh.target(mesh.next(mesh.opposite(hf))));
    total_cost += 1.0 - internal::dihedral_angle_cosine(p1, p2, p3, p4);
  }
  return total_cost;
}

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces
>>>>>>> main
