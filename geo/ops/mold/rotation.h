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

    double s_phi, c_phi, w_p;
    CGAL::rational_rotation_approximation(-phi, s_phi, c_phi, w_p, 1.0, 1000000.0);

    EK::RT sin_phi(std::llround(s_phi));
    EK::RT cos_phi(std::llround(c_phi));
    EK::RT w_phi(std::llround(w_p));

    CGAL::Aff_transformation_3<EK> Rz(
        cos_phi, -sin_phi, 0, 0,
        sin_phi, cos_phi, 0, 0,
        0, 0, w_phi, 0,
        w_phi
    );

    CGAL::Aff_transformation_3<EK> Rz_inv(
        cos_phi, sin_phi, 0, 0,
        -sin_phi, cos_phi, 0, 0,
        0, 0, w_phi, 0,
        w_phi
    );

    double s_theta, c_theta, w_t;
    CGAL::rational_rotation_approximation(-theta, s_theta, c_theta, w_t, 1.0, 1000000.0);

    EK::RT sin_theta(std::llround(s_theta));
    EK::RT cos_theta(std::llround(c_theta));
    EK::RT w_theta(std::llround(w_t));

    CGAL::Aff_transformation_3<EK> Ry(
        cos_theta, 0, sin_theta, 0,
        0, w_theta, 0, 0,
        -sin_theta, 0, cos_theta, 0,
        w_theta
    );
    CGAL::Aff_transformation_3<EK> Ry_inv(
        cos_theta, 0, -sin_theta, 0,
        0, w_theta, 0, 0,
        sin_theta, 0, cos_theta, 0,
        w_theta
    );

    CGAL::Aff_transformation_3<EK> to_z = Ry * Rz;
    CGAL::Aff_transformation_3<EK> from_z = Rz_inv * Ry_inv;
    return {to_z, from_z};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
