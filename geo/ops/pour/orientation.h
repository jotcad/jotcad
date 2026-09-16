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
    const std::vector<FT>& face_areas,
    double min_angle_turns = default_min_angle
) {
    std::vector<EK::Vector_3> candidate_dirs;

    // 1. Cardinal axes (6 directions)
    for (int s : {-1, 1}) {
        candidate_dirs.push_back(EK::Vector_3(s, 0, 0));
        candidate_dirs.push_back(EK::Vector_3(0, s, 0));
        candidate_dirs.push_back(EK::Vector_3(0, 0, s));
    }

    // 2. Corner diagonals (8 directions)
    for (int sx : {-1, 1}) {
        for (int sy : {-1, 1}) {
            for (int sz : {-1, 1}) {
                candidate_dirs.push_back(EK::Vector_3(sx, sy, sz));
            }
        }
    }

    // 3. Planar Edge diagonals (12 directions)
    for (int s1 : {-1, 1}) {
        for (int s2 : {-1, 1}) {
            candidate_dirs.push_back(EK::Vector_3(s1, s2, 0));
            candidate_dirs.push_back(EK::Vector_3(s1, 0, s2));
            candidate_dirs.push_back(EK::Vector_3(0, s1, s2));
        }
    }

    // 4. Face normals (dominant model facets)
    for (const auto& fn : face_normals) {
        double fn_len = std::sqrt(CGAL::to_double(fn.squared_length()));
        if (fn_len > 1e-6) {
            candidate_dirs.push_back(fn);
            candidate_dirs.push_back(-fn);
        }
    }

    // 5. Canonical Compound Tangent Offsets:
    // For every primary candidate d0 (cardinals, planar diagonals, corner diagonals),
    // construct an orthonormal tangent frame (u, v) and generate orthogonal compound roll offsets.
    // An offset of delta = 6° (~0.1045) tilts adjacent perpendicular features across the 5° bubble detachment threshold.
    const double delta_rad = 6.0 * M_PI / 180.0;
    const double tan_delta = std::tan(delta_rad);

    std::vector<EK::Vector_3> primary_bases = candidate_dirs;
    for (const auto& raw_base : primary_bases) {
        double len = std::sqrt(CGAL::to_double(raw_base.squared_length()));
        if (len < 1e-6) continue;
        double bx = CGAL::to_double(raw_base.x()) / len;
        double by = CGAL::to_double(raw_base.y()) / len;
        double bz = CGAL::to_double(raw_base.z()) / len;

        // Construct orthonormal tangent frame (u, v) on S²
        double ux, uy, uz;
        if (std::abs(bz) < 0.9) {
            double u_len = std::sqrt(bx * bx + by * by);
            ux = -by / u_len;
            uy = bx / u_len;
            uz = 0.0;
        } else {
            double u_len = std::sqrt(bx * bx + bz * bz);
            ux = -bz / u_len;
            uy = 0.0;
            uz = bx / u_len;
        }
        double vx = by * uz - bz * uy;
        double vy = bz * ux - bx * uz;
        double vz = bx * uy - by * ux;

        // Single-axis compound offsets: ±6° in u, ±6° in v
        for (double su : {-tan_delta, tan_delta}) {
            candidate_dirs.push_back(EK::Vector_3(FT(bx + su * ux), FT(by + su * uy), FT(bz + su * uz)));
        }
        for (double sv : {-tan_delta, tan_delta}) {
            candidate_dirs.push_back(EK::Vector_3(FT(bx + sv * vx), FT(by + sv * vy), FT(bz + sv * vz)));
        }
        // Dual-axis compound offsets: ±6° in u AND ±6° in v
        for (double su : {-tan_delta, tan_delta}) {
            for (double sv : {-tan_delta, tan_delta}) {
                candidate_dirs.push_back(EK::Vector_3(
                    FT(bx + su * ux + sv * vx),
                    FT(by + su * uy + sv * vy),
                    FT(bz + su * uz + sv * vz)
                ));
            }
        }
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

    // Lexicographic Ranking Structure (ZERO arbitrary weights)
    struct OrientationRank {
        int peak_count = 999999;       // Tier 1: True hydraulic traps / peaks (fewer is strictly better)
        double flat_area = 1e18;       // Tier 2: Flat ceiling area with α < min_angle (mm², lower is strictly better)
        double height_span = 1e18;     // Tier 3: Vertical bounding span Z_max - Z_min (mm, lower is strictly better)
        EK::Vector_3 dir = EK::Vector_3(0, 0, 1);

        bool is_better_than(const OrientationRank& o) const {
            if (peak_count != o.peak_count) {
                return peak_count < o.peak_count; // Tier 1 dominates unconditionally
            }
            if (std::abs(flat_area - o.flat_area) > 1e-4) {
                return flat_area < o.flat_area;   // Tier 2 tie-breaker
            }
            return height_span < o.height_span;   // Tier 3 tie-breaker
        }
    };

    OrientationRank best_rank;
    double sin_min = std::sin(min_angle_turns * 2.0 * M_PI);

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

        // 2. Count plateau-aware local summits (Tier 1)
        std::vector<bool> visited(pts.size(), false);
        int peak_count = 0;

        for (size_t i = 0; i < pts.size(); ++i) {
            if (visited[i]) continue;

            // BFS across connected vertices of equal height (plateau)
            std::vector<int> component;
            std::queue<int> q;
            q.push((int)i);
            visited[i] = true;
            double plateau_h = heights[i];
            bool has_higher_neighbor = false;

            while (!q.empty()) {
                int curr = q.front();
                q.pop();
                component.push_back(curr);

                for (int n_idx : neighbors[curr]) {
                    double nh = heights[n_idx];
                    if (nh > plateau_h + 1e-6) {
                        has_higher_neighbor = true;
                    } else if (std::abs(nh - plateau_h) <= 1e-6 && !visited[n_idx]) {
                        visited[n_idx] = true;
                        q.push(n_idx);
                    }
                }
            }

            // A plateau is a peak if and only if NO neighbor is strictly higher
            if (!has_higher_neighbor) {
                peak_count++;
            }
        }

        // 3. Calculate flat ceiling area with α < min_angle (Tier 2)
        double flat_ceiling_area = 0.0;
        for (size_t f_idx = 0; f_idx < face_normals.size(); ++f_idx) {
            const auto& fn = face_normals[f_idx];
            double fn_len = std::sqrt(CGAL::to_double(fn.squared_length()));
            if (fn_len < 1e-9) continue;

            double dot_up = CGAL::to_double((fn.x() * u_dir.x() + fn.y() * u_dir.y() + fn.z() * u_dir.z()) / FT(fn_len));
            double area = CGAL::to_double(face_areas[f_idx]);

            // Upward cavity ceiling facet
            if (dot_up > 0.001) {
                double sin_phi = std::sqrt((std::max)(0.0, 1.0 - dot_up * dot_up));
                if (sin_phi < sin_min) { // slope < min_angle: bubble stagnation hazard
                    flat_ceiling_area += area;
                }
            }
        }

        double height_span = max_h - min_h; // Tier 3

        OrientationRank current_rank;
        current_rank.peak_count = peak_count;
        current_rank.flat_area = flat_ceiling_area;
        current_rank.height_span = height_span;
        current_rank.dir = u_dir;

        if (current_rank.is_better_than(best_rank)) {
            best_rank = current_rank;
        }
    }

    std::cout << "  [Pour Orientation] Evaluated " << candidate_dirs.size() 
              << " candidate up-vectors (min_angle=" << min_angle_turns << " turns)." << std::endl
              << "    Best Rank -> Peaks: " << best_rank.peak_count 
              << ", Flat Ceiling Area: " << best_rank.flat_area << " mm²"
              << ", Height Span: " << best_rank.height_span << " mm"
              << ", Dir: (" << CGAL::to_double(best_rank.dir.x()) << ", "
              << CGAL::to_double(best_rank.dir.y()) << ", "
              << CGAL::to_double(best_rank.dir.z()) << ")" << std::endl << std::flush;

    return best_rank.dir;
}

inline Transformation get_gravity_rotation(const EK::Vector_3& up_dir) {
    double len = std::sqrt(CGAL::to_double(up_dir.squared_length()));
    if (len < 1e-9) {
        return Transformation(CGAL::IDENTITY);
    }
    EK::Vector_3 z_axis(up_dir.x() / FT(len), up_dir.y() / FT(len), up_dir.z() / FT(len));

    double dot = CGAL::to_double(z_axis.z());
    if (dot > 0.999999) {
        return Transformation(CGAL::IDENTITY); // Already aligned with +Z
    }

    EK::Vector_3 ref(0, 1, 0);
    if (std::abs(CGAL::to_double(z_axis.y())) > 0.9) {
        ref = EK::Vector_3(1, 0, 0);
    }

    EK::Vector_3 x_axis = CGAL::cross_product(ref, z_axis);
    double x_len = std::sqrt(CGAL::to_double(x_axis.squared_length()));
    if (x_len < 1e-9) {
        return Transformation(CGAL::IDENTITY);
    }
    x_axis = EK::Vector_3(x_axis.x() / FT(x_len), x_axis.y() / FT(x_len), x_axis.z() / FT(x_len));

    EK::Vector_3 y_axis = CGAL::cross_product(z_axis, x_axis);

    return Transformation(
        x_axis.x(), x_axis.y(), x_axis.z(), FT(0),
        y_axis.x(), y_axis.y(), y_axis.z(), FT(0),
        z_axis.x(), z_axis.y(), z_axis.z(), FT(0)
    );
}

inline ExactMesh rotate_mesh_to_gravity(const ExactMesh& mesh, const Transformation& rot_tf) {
    if (rot_tf == Transformation(CGAL::IDENTITY)) {
        return mesh;
    }
    ExactMesh oriented_mesh = mesh;
    for (auto v : oriented_mesh.vertices()) {
        oriented_mesh.point(v) = rot_tf.transform(oriented_mesh.point(v));
    }
    return oriented_mesh;
}

inline ExactMesh rotate_mesh_to_gravity(const ExactMesh& mesh, const EK::Vector_3& up_dir) {
    Transformation rot_tf = get_gravity_rotation(up_dir);
    return rotate_mesh_to_gravity(mesh, rot_tf);
}

} // namespace pour
} // namespace geo
} // namespace jotcad
