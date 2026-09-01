#pragma once
#include "types.h"
#include <CGAL/rational_rotation.h>

namespace jotcad {
namespace geo {
namespace mold {

// Computes exact rational forward (to_z) and inverse (from_z) transformations to align direction d with +Z
inline std::pair<CGAL::Aff_transformation_3<EK>, CGAL::Aff_transformation_3<EK>> compute_exact_z_rotation(const EK::Vector_3& d) {
    double dx_d = CGAL::to_double(d.x());
    double dy_d = CGAL::to_double(d.y());
    double dz_d = CGAL::to_double(d.z());

    double phi = std::atan2(dy_d, dx_d);
    double theta = std::atan2(std::sqrt(dx_d * dx_d + dy_d * dy_d), dz_d);

    EK::RT sin_phi, cos_phi, w_phi;
    CGAL::rational_rotation_approximation(-phi, sin_phi, cos_phi, w_phi, EK::RT(1), EK::RT(1000000));
    CGAL::Aff_transformation_3<EK> Rz(
        cos_phi, -sin_phi, 0, 0,
        sin_phi, cos_phi, 0, 0,
        0, 0, w_phi, 0,
        w_phi
    );

    EK::RT sin_theta, cos_theta, w_theta;
    CGAL::rational_rotation_approximation(-theta, sin_theta, cos_theta, w_theta, EK::RT(1), EK::RT(1000000));
    CGAL::Aff_transformation_3<EK> Ry(
        cos_theta, 0, sin_theta, 0,
        0, w_theta, 0, 0,
        -sin_theta, 0, cos_theta, 0,
        w_theta
    );

    CGAL::Aff_transformation_3<EK> to_z = Ry * Rz;
    CGAL::Aff_transformation_3<EK> from_z = to_z.inverse();
    return {to_z, from_z};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
