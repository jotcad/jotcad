#pragma once
#include "types.h"
#include <cmath>

namespace jotcad {
namespace geo {
namespace mold {

inline Geometry build_oriented_prism_geo(
    const std::vector<EK::Point_3>& patch_pts,
    const EK::Vector_3& draw_dir,
    FT margin_pad,
    FT ext_length
) {
    double vx = CGAL::to_double(draw_dir.x());
    double vy = CGAL::to_double(draw_dir.y());
    double vz = CGAL::to_double(draw_dir.z());
    double len = std::sqrt(vx*vx + vy*vy + vz*vz);
    if (len > 1e-12) { vx /= len; vy /= len; vz /= len; }
    else { vx = 0; vy = 0; vz = -1; }

    double R[3][3];
    if (std::abs(vz - 1.0) < 1e-9) {
        R[0][0]=1; R[0][1]=0; R[0][2]=0;
        R[1][0]=0; R[1][1]=1; R[1][2]=0;
        R[2][0]=0; R[2][1]=0; R[2][2]=1;
    } else if (std::abs(vz + 1.0) < 1e-9) {
        R[0][0]=1; R[0][1]=0; R[0][2]=0;
        R[1][0]=0; R[1][1]=-1; R[1][2]=0;
        R[2][0]=0; R[2][1]=0; R[2][2]=-1;
    } else {
        double ax = -vy, ay = vx;
        double alen = std::sqrt(ax*ax + ay*ay);
        ax /= alen; ay /= alen;
        double c = vz;
        double s = std::sqrt(std::max(0.0, 1.0 - c*c));
        double oc = 1.0 - c;
        R[0][0] = c + ax*ax*oc;    R[0][1] = ax*ay*oc;        R[0][2] = ay*s;
        R[1][0] = ay*ax*oc;        R[1][1] = c + ay*ay*oc;    R[1][2] = -ax*s;
        R[2][0] = -ay*s;           R[2][1] = ax*s;            R[2][2] = c;
    }

    auto world_to_local = [&](double wx, double wy, double wz) -> EK::Point_3 {
        double lx = R[0][0]*wx + R[1][0]*wy + R[2][0]*wz;
        double ly = R[0][1]*wx + R[1][1]*wy + R[2][1]*wz;
        double lz = R[0][2]*wx + R[1][2]*wy + R[2][2]*wz;
        return EK::Point_3(FT(lx), FT(ly), FT(lz));
    };

    auto local_to_world = [&](double lx, double ly, double lz) -> Vertex {
        double wx = R[0][0]*lx + R[0][1]*ly + R[0][2]*lz;
        double wy = R[1][0]*lx + R[1][1]*ly + R[1][2]*lz;
        double wz = R[2][0]*lx + R[2][1]*ly + R[2][2]*lz;
        return Vertex{FT(wx), FT(wy), FT(wz)};
    };

    double l_xmin = 1e9, l_xmax = -1e9;
    double l_ymin = 1e9, l_ymax = -1e9;
    double l_zmin = 1e9, l_zmax = -1e9;

    for (const auto& pt : patch_pts) {
        EK::Point_3 loc = world_to_local(CGAL::to_double(pt.x()), CGAL::to_double(pt.y()), CGAL::to_double(pt.z()));
        double lx = CGAL::to_double(loc.x()), ly = CGAL::to_double(loc.y()), lz = CGAL::to_double(loc.z());
        l_xmin = std::min(l_xmin, lx); l_xmax = std::max(l_xmax, lx);
        l_ymin = std::min(l_ymin, ly); l_ymax = std::max(l_ymax, ly);
        l_zmin = std::min(l_zmin, lz); l_zmax = std::max(l_zmax, lz);
    }

    double pad = CGAL::to_double(margin_pad);
    l_xmin -= pad; l_xmax += pad;
    l_ymin -= pad; l_ymax += pad;
    double l_zmax_ext = l_zmax + CGAL::to_double(ext_length);

    Geometry geo;
    geo.vertices.push_back(local_to_world(l_xmin, l_ymin, l_zmin)); // 0
    geo.vertices.push_back(local_to_world(l_xmax, l_ymin, l_zmin)); // 1
    geo.vertices.push_back(local_to_world(l_xmax, l_ymax, l_zmin)); // 2
    geo.vertices.push_back(local_to_world(l_xmin, l_ymax, l_zmin)); // 3
    geo.vertices.push_back(local_to_world(l_xmin, l_ymin, l_zmax_ext)); // 4
    geo.vertices.push_back(local_to_world(l_xmax, l_ymin, l_zmax_ext)); // 5
    geo.vertices.push_back(local_to_world(l_xmax, l_ymax, l_zmax_ext)); // 6
    geo.vertices.push_back(local_to_world(l_xmin, l_ymax, l_zmax_ext)); // 7

    geo.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
    geo.faces.push_back({{{4, 5, 6, 7}}}); // Top
    geo.faces.push_back({{{0, 1, 5, 4}}}); // Front
    geo.faces.push_back({{{1, 2, 6, 5}}}); // Right
    geo.faces.push_back({{{2, 3, 7, 6}}}); // Back
    geo.faces.push_back({{{3, 0, 4, 7}}}); // Left

    geo.triangles.push_back({3, 2, 1}); geo.triangles.push_back({3, 1, 0});
    geo.triangles.push_back({4, 5, 6}); geo.triangles.push_back({4, 6, 7});
    geo.triangles.push_back({0, 1, 5}); geo.triangles.push_back({0, 5, 4});
    geo.triangles.push_back({1, 2, 6}); geo.triangles.push_back({1, 6, 5});
    geo.triangles.push_back({2, 3, 7}); geo.triangles.push_back({2, 7, 6});
    geo.triangles.push_back({3, 0, 4}); geo.triangles.push_back({3, 4, 7});
    return geo;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
