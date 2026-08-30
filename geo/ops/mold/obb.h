#pragma once
#include "types.h"
#include <cmath>

namespace jotcad {
namespace geo {
namespace mold {

struct OrientedBox {
    EK::Vector_3 u, v, w;
    FT u_min, u_max;
    FT v_min, v_max;
    FT w_min, w_max;
    FT volume = FT(0);

    Geometry to_geometry() const {
        auto make_pt = [&](FT u_val, FT v_val, FT w_val) -> Vertex {
            FT px = u_val * u.x() + v_val * v.x() + w_val * w.x();
            FT py = u_val * u.y() + v_val * v.y() + w_val * w.y();
            FT pz = u_val * u.z() + v_val * v.z() + w_val * w.z();
            return Vertex{px, py, pz};
        };

        Geometry geo;
        geo.vertices.push_back(make_pt(u_min, v_min, w_min)); // 0
        geo.vertices.push_back(make_pt(u_max, v_min, w_min)); // 1
        geo.vertices.push_back(make_pt(u_max, v_max, w_min)); // 2
        geo.vertices.push_back(make_pt(u_min, v_max, w_min)); // 3
        geo.vertices.push_back(make_pt(u_min, v_min, w_max)); // 4
        geo.vertices.push_back(make_pt(u_max, v_min, w_max)); // 5
        geo.vertices.push_back(make_pt(u_max, v_max, w_max)); // 6
        geo.vertices.push_back(make_pt(u_min, v_max, w_max)); // 7

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
};

inline OrientedBox compute_min_volume_obb(
    const ExactMesh& mesh,
    FT padding,
    const std::vector<EK::Vector_3>& preferred_dirs = {}
) {
    std::vector<EK::Point_3> pts;
    for (auto v : mesh.vertices()) {
        pts.push_back(mesh.point(v));
    }

    std::vector<EK::Vector_3> candidate_u;
    for (const auto& d : preferred_dirs) {
        double len = std::sqrt(CGAL::to_double(d.squared_length()));
        if (len > 1e-9) {
            candidate_u.push_back(EK::Vector_3(d.x() / FT(len), d.y() / FT(len), d.z() / FT(len)));
        }
    }

    candidate_u.push_back(EK::Vector_3(1, 0, 0));
    candidate_u.push_back(EK::Vector_3(0, 1, 0));
    candidate_u.push_back(EK::Vector_3(0, 0, 1));

    const int N_SPHERE = 32;
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    for (int i = 0; i < N_SPHERE; ++i) {
        double y = 1.0 - (i / double(N_SPHERE - 1)) * 2.0;
        double radius = std::sqrt(std::max(0.0, 1.0 - y * y));
        double theta = 2.0 * M_PI * i / phi;
        double x = std::cos(theta) * radius;
        double z = std::sin(theta) * radius;
        candidate_u.push_back(EK::Vector_3(FT(x), FT(y), FT(z)));
    }

    OrientedBox best_box;
    FT min_vol = -1;

    for (const auto& u_raw : candidate_u) {
        double u_len = std::sqrt(CGAL::to_double(u_raw.squared_length()));
        if (u_len < 1e-9) continue;
        EK::Vector_3 u(u_raw.x() / FT(u_len), u_raw.y() / FT(u_len), u_raw.z() / FT(u_len));

        EK::Vector_3 ref(0, 0, 1);
        if (std::abs(CGAL::to_double(u.z())) > 0.9) {
            ref = EK::Vector_3(1, 0, 0);
        }
        EK::Vector_3 v_base = CGAL::cross_product(u, ref);
        double v_len = std::sqrt(CGAL::to_double(v_base.squared_length()));
        if (v_len < 1e-9) continue;
        v_base = EK::Vector_3(v_base.x() / FT(v_len), v_base.y() / FT(v_len), v_base.z() / FT(v_len));
        EK::Vector_3 w_base = CGAL::cross_product(u, v_base);

        const int NUM_ANGLES = 12;
        for (int a = 0; a < NUM_ANGLES; ++a) {
            double angle = (a * M_PI) / double(NUM_ANGLES);
            double cos_a = std::cos(angle);
            double sin_a = std::sin(angle);

            EK::Vector_3 v = v_base * FT(cos_a) + w_base * FT(sin_a);
            EK::Vector_3 w = CGAL::cross_product(u, v);

            FT u_min = 1000000, u_max = -1000000;
            FT v_min = 1000000, v_max = -1000000;
            FT w_min = 1000000, w_max = -1000000;

            for (const auto& p : pts) {
                EK::Vector_3 pv(p.x(), p.y(), p.z());
                FT pu = pv * u;
                FT pv_val = pv * v;
                FT pw = pv * w;

                if (pu < u_min) u_min = pu;
                if (pu > u_max) u_max = pu;
                if (pv_val < v_min) v_min = pv_val;
                if (pv_val > v_max) v_max = pv_val;
                if (pw < w_min) w_min = pw;
                if (pw > w_max) w_max = pw;
            }

            u_min = u_min - padding; u_max = u_max + padding;
            v_min = v_min - padding; v_max = v_max + padding;
            w_min = w_min - padding; w_max = w_max + padding;

            FT vol = (u_max - u_min) * (v_max - v_min) * (w_max - w_min);
            if (min_vol < FT(0) || vol < min_vol) {
                min_vol = vol;
                best_box.u = u; best_box.v = v; best_box.w = w;
                best_box.u_min = u_min; best_box.u_max = u_max;
                best_box.v_min = v_min; best_box.v_max = v_max;
                best_box.w_min = w_min; best_box.w_max = w_max;
                best_box.volume = vol;
            }
        }
    }

    return best_box;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
