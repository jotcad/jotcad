#pragma once
#include "kernel.h"
#include <vector>
#include <cmath>

namespace jotcad {
namespace geo {

struct Frame {
    Point_3 position;
    Vector_3 tangent;
    Vector_3 normal;
    Vector_3 binormal;

    Point_3 transform(const Point_3& p) const {
        // Transforms local profile point (x, y, 0) into world space
        // Local X -> Normal
        // Local Y -> Binormal
        // Local Z -> Tangent (assuming z=0 for profiles usually)
        return position + CGAL::to_double(p.x()) * normal +
               CGAL::to_double(p.y()) * binormal + CGAL::to_double(p.z()) * tangent;
    }

    void twist(double theta) {
        double c = std::cos(theta);
        double s = std::sin(theta);
        Vector_3 new_n = c * normal + s * binormal;
        Vector_3 new_b = -s * normal + c * binormal;
        normal = new_n;
        binormal = new_b;
    }
};

/**
 * generate_rmf: Generates Rotation Minimizing Frames along a path using the Double Reflection method.
 */
static std::vector<Frame> generate_rmf(const std::vector<Point_3>& path, bool closed = false) {
    std::vector<Frame> frames;
    if (path.size() < 2) return frames;

    frames.reserve(path.size());

    // 1. Initial Frame (Point 0)
    Point_3 p0 = path[0];
    Point_3 p1 = path[1];
    Vector_3 t0 = p1 - p0;
    double t0_len_sq = CGAL::to_double(t0.squared_length());
    if (t0_len_sq > 0) t0 = t0 / std::sqrt(t0_len_sq);

    Vector_3 n0;
    if (std::abs(CGAL::to_double(t0.x())) < 0.9) {
        n0 = CGAL::cross_product(t0, Vector_3(1, 0, 0));
    } else {
        n0 = CGAL::cross_product(t0, Vector_3(0, 1, 0));
    }
    double n0_len_sq = CGAL::to_double(n0.squared_length());
    if (n0_len_sq > 0) n0 = n0 / std::sqrt(n0_len_sq);
    Vector_3 b0 = CGAL::cross_product(t0, n0);
    frames.push_back({p0, t0, n0, b0});

    // 2. Propagate Frames (Double Reflection)
    for (size_t i = 1; i < path.size(); ++i) {
        Point_3 prev_p = path[i - 1];
        Point_3 curr_p = path[i];
        Vector_3 prev_t = frames.back().tangent;
        Vector_3 prev_n = frames.back().normal;

        Vector_3 v1 = curr_p - prev_p;
        double c1 = CGAL::to_double(v1.squared_length());
        if (c1 == 0) {
            frames.push_back(frames.back());
            frames.back().position = curr_p;
            continue;
        }

        Vector_3 n_prev_reflected = prev_n - (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_n)) * v1;
        Vector_3 t_prev_reflected = prev_t - (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_t)) * v1;

        Vector_3 next_t;
        if (i < path.size() - 1) {
            next_t = path[i + 1] - curr_p;
        } else if (closed) {
            next_t = path[0] - curr_p;
        } else {
            next_t = prev_t;
        }
        double next_t_len_sq = CGAL::to_double(next_t.squared_length());
        if (next_t_len_sq > 0) next_t = next_t / std::sqrt(next_t_len_sq);

        Vector_3 v2 = next_t - t_prev_reflected;
        double c2 = CGAL::to_double(v2.squared_length());

        Vector_3 next_n;
        if (c2 == 0) {
            next_n = n_prev_reflected;
        } else {
            next_n = n_prev_reflected - (2.0 / c2) * CGAL::to_double(CGAL::scalar_product(v2, n_prev_reflected)) * v2;
        }
        
        double nn_len_sq = CGAL::to_double(next_n.squared_length());
        if (nn_len_sq > 0) next_n = next_n / std::sqrt(nn_len_sq);
        Vector_3 next_b = CGAL::cross_product(next_t, next_n);
        frames.push_back({curr_p, next_t, next_n, next_b});
    }

    // 3. Handle Closure (Twist correction)
    if (closed && frames.size() > 1) {
        Point_3 prev_p = path.back();
        Point_3 curr_p = path[0];
        Vector_3 prev_t = frames.back().tangent;
        Vector_3 prev_n = frames.back().normal;
        Vector_3 v1 = curr_p - prev_p;
        double c1 = CGAL::to_double(v1.squared_length());
        if (c1 > 0) {
            Vector_3 n_reflected = prev_n - (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_n)) * v1;
            Vector_3 t_reflected = prev_t - (2.0 / c1) * CGAL::to_double(CGAL::scalar_product(v1, prev_t)) * v1;
            Vector_3 target_t = frames[0].tangent;
            Vector_3 v2 = target_t - t_reflected;
            double c2 = CGAL::to_double(v2.squared_length());
            Vector_3 final_n = (c2 == 0) ? n_reflected : n_reflected - (2.0 / c2) * CGAL::to_double(CGAL::scalar_product(v2, n_reflected)) * v2;
            
            Vector_3 target_n = frames[0].normal;
            double dot = CGAL::to_double(CGAL::scalar_product(final_n, target_n));
            if (dot > 1.0) dot = 1.0;
            if (dot < -1.0) dot = -1.0;
            double angle = std::acos(dot);
            Vector_3 cross = CGAL::cross_product(final_n, target_n);
            if (CGAL::to_double(CGAL::scalar_product(cross, target_t)) < 0) angle = -angle;
            
            for (size_t i = 0; i < frames.size(); ++i) {
                frames[i].twist(((double)i / (double)frames.size()) * angle);
            }
        }
    }
    return frames;
}

} // namespace geo
} // namespace jotcad
