#pragma once
#include "types.h"
#include <CGAL/convex_hull_3.h>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace pour {

inline EK::Vector_3 find_optimal_pour_orientation(
    const ExactMesh& mesh,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas
) {
    std::vector<EK::Vector_3> candidate_dirs;

    // 1. Cardinal axes
    candidate_dirs.push_back(EK::Vector_3(1, 0, 0));
    candidate_dirs.push_back(EK::Vector_3(-1, 0, 0));
    candidate_dirs.push_back(EK::Vector_3(0, 1, 0));
    candidate_dirs.push_back(EK::Vector_3(0, -1, 0));
    candidate_dirs.push_back(EK::Vector_3(0, 0, 1));
    candidate_dirs.push_back(EK::Vector_3(0, 0, -1));

    // 2. Corner diagonals
    candidate_dirs.push_back(EK::Vector_3(1, 1, 1));
    candidate_dirs.push_back(EK::Vector_3(1, 1, -1));
    candidate_dirs.push_back(EK::Vector_3(1, -1, 1));
    candidate_dirs.push_back(EK::Vector_3(1, -1, -1));
    candidate_dirs.push_back(EK::Vector_3(-1, 1, 1));
    candidate_dirs.push_back(EK::Vector_3(-1, 1, -1));
    candidate_dirs.push_back(EK::Vector_3(-1, -1, 1));
    candidate_dirs.push_back(EK::Vector_3(-1, -1, -1));

    // 3. Face normals
    for (const auto& fn : face_normals) {
        candidate_dirs.push_back(fn);
        candidate_dirs.push_back(-fn);
    }

    // 4. Fibonacci spherical grid
    const int N_SPHERE = 150;
    const double phi = (1.0 + std::sqrt(5.0)) / 2.0;
    for (int i = 0; i < N_SPHERE; ++i) {
        double y = 1.0 - (i / double(N_SPHERE - 1)) * 2.0;
        double radius = std::sqrt((std::max)(0.0, 1.0 - y * y));
        double theta = 2.0 * M_PI * i / phi;
        double x = std::cos(theta) * radius;
        double z = std::sin(theta) * radius;
        candidate_dirs.push_back(EK::Vector_3(FT(x), FT(y), FT(z)));
    }

    // Pre-extract vertices and adjacency
    std::vector<EK::Point_3> pts;
    pts.reserve(mesh.number_of_vertices());
    for (auto v : mesh.vertices()) {
        pts.push_back(mesh.point(v));
    }

    std::vector<std::vector<int>> neighbors(mesh.number_of_vertices());
    for (auto e : mesh.edges()) {
        auto h = mesh.halfedge(e);
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        neighbors[u].push_back(v);
        neighbors[v].push_back(u);
    }

    double best_score = 1e18;
    EK::Vector_3 best_dir(0, 0, 1);

    for (const auto& raw_dir : candidate_dirs) {
        double len = std::sqrt(CGAL::to_double(raw_dir.squared_length()));
        if (len < 1e-9) continue;
        EK::Vector_3 u_dir(raw_dir.x() / FT(len), raw_dir.y() / FT(len), raw_dir.z() / FT(len));

        // 1. Calculate projected heights
        std::vector<double> heights(pts.size());
        double min_h = 1e18, max_h = -1e18;
        for (size_t i = 0; i < pts.size(); ++i) {
            double h = CGAL::to_double(pts[i].x() * u_dir.x() + pts[i].y() * u_dir.y() + pts[i].z() * u_dir.z());
            heights[i] = h;
            if (h < min_h) min_h = h;
            if (h > max_h) max_h = h;
        }

        // 2. Count 1-ring local peaks
        int peak_count = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            bool is_peak = true;
            for (int n_idx : neighbors[i]) {
                if (heights[n_idx] > heights[i] + 1e-6) {
                    is_peak = false;
                    break;
                }
            }
            if (is_peak) peak_count++;
        }

        // 3. Calculate ceiling slope penalties
        double ceiling_penalty = 0.0;
        for (size_t f_idx = 0; f_idx < face_normals.size(); ++f_idx) {
            const auto& fn = face_normals[f_idx];
            double fn_len = std::sqrt(CGAL::to_double(fn.squared_length()));
            if (fn_len < 1e-9) continue;

            double dot_up = CGAL::to_double((fn.x()*u_dir.x() + fn.y()*u_dir.y() + fn.z()*u_dir.z()) / FT(fn_len));
            double area = CGAL::to_double(face_areas[f_idx]);

            // Ceiling surface (normal facing downward)
            if (dot_up < -0.05) {
                double sin_phi = std::sqrt((std::max)(0.0, 1.0 - dot_up * dot_up));
                if (sin_phi < 0.25) { // slope < 15 degrees: flat ceiling stagnation trap
                    ceiling_penalty += area * 100.0;
                } else {
                    ceiling_penalty += area * (1.0 - sin_phi) * 2.0;
                }
            }
        }

        double height_span = max_h - min_h;
        double score = double(peak_count) * 1000.0 + ceiling_penalty - height_span * 0.1;

        if (score < best_score) {
            best_score = score;
            best_dir = u_dir;
        }
    }

    return best_dir;
}

inline ExactMesh rotate_mesh_to_gravity(const ExactMesh& mesh, const EK::Vector_3& up_dir) {
    double len = std::sqrt(CGAL::to_double(up_dir.squared_length()));
    EK::Vector_3 z_axis(up_dir.x() / FT(len), up_dir.y() / FT(len), up_dir.z() / FT(len));

    EK::Vector_3 target_z(0, 0, 1);
    double dot = CGAL::to_double(z_axis.z());

    if (dot > 0.999999) {
        return mesh; // Already aligned with +Z
    }

    EK::Vector_3 ref(0, 1, 0);
    if (std::abs(CGAL::to_double(z_axis.y())) > 0.9) {
        ref = EK::Vector_3(1, 0, 0);
    }

    EK::Vector_3 x_axis = CGAL::cross_product(ref, z_axis);
    double x_len = std::sqrt(CGAL::to_double(x_axis.squared_length()));
    x_axis = EK::Vector_3(x_axis.x() / FT(x_len), x_axis.y() / FT(x_len), x_axis.z() / FT(x_len));

    EK::Vector_3 y_axis = CGAL::cross_product(z_axis, x_axis);

    ExactMesh oriented_mesh = mesh;
    for (auto v : oriented_mesh.vertices()) {
        auto p = oriented_mesh.point(v);
        EK::Vector_3 pv(p.x(), p.y(), p.z());
        FT rx = pv * x_axis;
        FT ry = pv * y_axis;
        FT rz = pv * z_axis;
        oriented_mesh.point(v) = EK::Point_3(rx, ry, rz);
    }

    return oriented_mesh;
}

} // namespace pour
} // namespace geo
} // namespace jotcad
