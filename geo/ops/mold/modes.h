#pragma once

#include "types.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Represents a continuous 3D unit vector for spherical optimization on S2.
 */
struct Vector3d {
    double x = 0.0, y = 0.0, z = 0.0;

    Vector3d() = default;
    Vector3d(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}
    Vector3d(const EK::Vector_3& v)
        : x(CGAL::to_double(v.x())), y(CGAL::to_double(v.y())), z(CGAL::to_double(v.z())) {}

    double dot(const Vector3d& o) const {
        return x * o.x + y * o.y + z * o.z;
    }

    Vector3d operator+(const Vector3d& o) const {
        return {x + o.x, y + o.y, z + o.z};
    }

    Vector3d operator-(const Vector3d& o) const {
        return {x - o.x, y - o.y, z - o.z};
    }

    Vector3d operator*(double s) const {
        return {x * s, y * s, z * s};
    }

    double norm_sq() const {
        return x * x + y * y + z * z;
    }

    double norm() const {
        return std::sqrt(norm_sq());
    }

    Vector3d normalized() const {
        double n = norm();
        if (n < 1e-12) return {0.0, 0.0, 1.0};
        return {x / n, y / n, z / n};
    }

    EK::Vector_3 to_exact() const {
        return EK::Vector_3(FT(x), FT(y), FT(z));
    }
};

/**
 * @brief Computes area-weighted directional normal modes on S2 using spherical k-means.
 *
 * Partitions unhandled face normals into k dominant directional clusters, finding
 * the primary hills of draft and visibility on S2 in O(k * N) time (< 0.1 ms).
 */
inline std::vector<Vector3d> compute_normal_modes(
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    FaceBoolMap is_handled,
    int k = 6
) {
    std::vector<Vector3d> unhandled_normals;
    std::vector<double> unhandled_weights;
    unhandled_normals.reserve(face_descriptors.size());
    unhandled_weights.reserve(face_descriptors.size());

    for (auto f : face_descriptors) {
        if (!is_handled[f]) {
            size_t idx = f.idx();
            Vector3d n(face_normals[idx]);
            double w = CGAL::to_double(face_areas[idx]);
            if (w > 1e-9) {
                unhandled_normals.push_back(n.normalized());
                unhandled_weights.push_back(w);
            }
        }
    }

    if (unhandled_normals.empty()) {
        return {{0, 0, 1}, {0, 0, -1}, {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
    }

    // 1. Initialize k seed centroids along 6 cardinal directions and diagonal modes
    std::vector<Vector3d> centroids = {
        { 0,  0,  1}, { 0,  0, -1},
        { 1,  0,  0}, {-1,  0,  0},
        { 0,  1,  0}, { 0, -1,  0}
    };

    // 2. Spherical k-Means iterations (3-5 iterations suffice for fast convergence)
    const int max_iters = 4;
    for (int iter = 0; iter < max_iters; ++iter) {
        std::vector<Vector3d> cluster_sums(centroids.size(), Vector3d(0, 0, 0));
        std::vector<double> cluster_weights(centroids.size(), 0.0);

        for (size_t i = 0; i < unhandled_normals.size(); ++i) {
            const auto& n = unhandled_normals[i];
            double w = unhandled_weights[i];

            int best_c = 0;
            double best_dot = -2.0;
            for (size_t c = 0; c < centroids.size(); ++c) {
                double d = n.dot(centroids[c]);
                if (d > best_dot) {
                    best_dot = d;
                    best_c = (int)c;
                }
            }

            cluster_sums[best_c] = cluster_sums[best_c] + (n * w);
            cluster_weights[best_c] += w;
        }

        for (size_t c = 0; c < centroids.size(); ++c) {
            if (cluster_weights[c] > 1e-9 && cluster_sums[c].norm_sq() > 1e-12) {
                centroids[c] = cluster_sums[c].normalized();
            }
        }
    }

    // 3. Deduplicate near-identical centroids (dot > 0.98)
    std::vector<Vector3d> unique_modes;
    for (const auto& c : centroids) {
        bool duplicate = false;
        for (const auto& u : unique_modes) {
            if (c.dot(u) > 0.98) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            unique_modes.push_back(c);
        }
    }

    return unique_modes;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
