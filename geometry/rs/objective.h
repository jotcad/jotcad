#pragma once

#include "types.h"

namespace ruled_surfaces {

struct Objective {
  virtual ~Objective() = default;
  virtual double calculate_cost(const Mesh& mesh) const = 0;
  virtual bool is_slg_compatible() const { return false; }
  virtual bool is_dlg_compatible() const { return false; }
  virtual double get_dihedral_cost(const PointCgal&, const PointCgal&,
                                   const PointCgal&, const PointCgal&) const {
    return 0.0;
  }
};

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces
>>>>>>> main
