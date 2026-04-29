#pragma once
#include <vector>
#include <array>
#include <string>
#include <optional>
#include <cmath>
#include <numeric>
#include <algorithm>
#include <map>
#include "kernel.h"

namespace jotcad {
namespace geo {

struct Matrix; // Forward declaration

struct Vertex {
    FT x, y, z;
};

/**
 * Geometry: The foundational geometry container.
 */
struct Geometry {
    struct Face {
        std::vector<std::vector<int>> loops; // Outer loop, then holes
    };

    std::vector<Vertex> vertices;
    std::vector<Face> faces;
    std::vector<int> points;
    std::vector<std::array<int, 2>> segments;
    std::vector<std::array<int, 3>> triangles;

    void apply_tf(const Matrix& m);

    // Exact rational serialization
    void encode_text(std::string& out) const {
        std::stringstream ss;
        ss << "V " << vertices.size() << "\n";
        for (const auto& v : vertices) {
            ss << v.x << " " << v.y << " " << v.z << "\n";
        }
        ss << "F " << faces.size() << "\n";
        for (const auto& f : faces) {
            ss << f.loops.size() << " ";
            for (const auto& loop : f.loops) {
                ss << loop.size() << " ";
                for (int idx : loop) ss << idx << " ";
            }
            ss << "\n";
        }
        ss << "P " << points.size() << "\n";
        for (int p : points) ss << p << " ";
        if (!points.empty()) ss << "\n";

        ss << "S " << segments.size() << "\n";
        for (const auto& s : segments) ss << s[0] << " " << s[1] << "\n";

        ss << "T " << triangles.size() << "\n";
        for (const auto& t : triangles) ss << t[0] << " " << t[1] << " " << t[2] << "\n";
        out = ss.str();
    }

    std::string encode_text() const {
        std::string out;
        encode_text(out);
        return out;
    }

    void decode_text(const std::string& in) {
        std::stringstream ss(in);
        std::string tag;
        while (ss >> tag) {
            if (tag == "V") {
                size_t count; ss >> count;
                vertices.resize(count);
                for (size_t i = 0; i < count; ++i) {
                    ss >> vertices[i].x >> vertices[i].y >> vertices[i].z;
                }
            } else if (tag == "F") {
                size_t count; ss >> count;
                faces.resize(count);
                for (size_t i = 0; i < count; ++i) {
                    size_t loops; ss >> loops;
                    faces[i].loops.resize(loops);
                    for (size_t j = 0; j < loops; ++j) {
                        size_t len; ss >> len;
                        faces[i].loops[j].resize(len);
                        for (size_t k = 0; k < len; ++k) ss >> faces[i].loops[j][k];
                    }
                }
            } else if (tag == "P") {
                size_t count; ss >> count;
                points.resize(count);
                for (size_t i = 0; i < count; ++i) ss >> points[i];
            } else if (tag == "S") {
                size_t count; ss >> count;
                segments.resize(count);
                for (size_t i = 0; i < count; ++i) ss >> segments[i][0] >> segments[i][1];
            } else if (tag == "T") {
                size_t count; ss >> count;
                triangles.resize(count);
                for (size_t i = 0; i < count; ++i) ss >> triangles[i][0] >> triangles[i][1] >> triangles[i][2];
            }
        }
    }

    std::optional<EK::Plane_3> find_plane() const {
        if (vertices.size() < 3) return std::nullopt;
        for (const auto& f : faces) {
            if (f.loops.empty() || f.loops[0].size() < 3) continue;
            auto v0 = vertices[f.loops[0][0]];
            auto v1 = vertices[f.loops[0][1]];
            auto v2 = vertices[f.loops[0][2]];
            EK::Plane_3 p(EK::Point_3(v0.x, v0.y, v0.z),
                          EK::Point_3(v1.x, v1.y, v1.z),
                          EK::Point_3(v2.x, v2.y, v2.z));
            if (!p.is_degenerate()) return p;
        }
        return std::nullopt;
    }

    bool is_plane() const {
        auto plane_opt = find_plane();
        if (!plane_opt) return false;
        return is_coplanar_with(*plane_opt);
    }

    bool is_coplanar_with(const EK::Plane_3& plane, double epsilon = 1e-6) const {
        for (const auto& v : vertices) {
            EK::Point_3 p(v.x, v.y, v.z);
            if (!plane.has_on(p)) {
                if (std::abs(CGAL::to_double(CGAL::squared_distance(plane, p))) > epsilon) {
                    return false;
                }
            }
        }
        return true;
    }
};

} // namespace geo
} // namespace jotcad
