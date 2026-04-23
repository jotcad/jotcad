#pragma once

#include <vector>
#include <string>
#include <array>
#include <sstream>
#include <iomanip>
#include <iostream>
#include "kernel.h"

namespace jotcad {
namespace geo {

struct Vertex {
    FT x, y, z;
};

/**
 * Geometry: The foundational exact geometric artifact in JOT.
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

    void apply_tf(const std::vector<std::string>& tf) {
        if (tf.size() < 16) return;
        auto parse = [](const std::string& s) {
            size_t slash = s.find('/');
            if (slash == std::string::npos) return FT(std::stod(s));
            return FT(std::stod(s.substr(0, slash))) / FT(std::stod(s.substr(slash + 1)));
        };
        Transformation t(parse(tf[0]), parse(tf[1]), parse(tf[2]), parse(tf[3]),
                         parse(tf[4]), parse(tf[5]), parse(tf[6]), parse(tf[7]),
                         parse(tf[8]), parse(tf[9]), parse(tf[10]), parse(tf[11]),
                         parse(tf[15]));
        for (auto& v : vertices) {
            Point_3 p(v.x, v.y, v.z);
            Point_3 tp = t.transform(p);
            v.x = tp.x(); v.y = tp.y(); v.z = tp.z();
        }
    }

    std::optional<EK::Plane_3> find_plane() const {
        if (vertices.size() < 3) return std::nullopt;
        // Simple 3-point plane for now (assuming clean input)
        for (size_t i = 0; i < vertices.size() - 2; ++i) {
            EK::Point_3 p1(vertices[i].x, vertices[i].y, vertices[i].z);
            EK::Point_3 p2(vertices[i+1].x, vertices[i+1].y, vertices[i+1].z);
            EK::Point_3 p3(vertices[i+2].x, vertices[i+2].y, vertices[i+2].z);
            if (!CGAL::collinear(p1, p2, p3)) {
                return EK::Plane_3(p1, p2, p3);
            }
        }
        return std::nullopt;
    }

    bool is_coplanar_with(const EK::Plane_3& plane, double epsilon = 1e-6) const {
        for (const auto& v : vertices) {
            EK::Point_3 p(v.x, v.y, v.z);
            if (CGAL::to_double(CGAL::squared_distance(plane, p)) > epsilon) {
                return false;
            }
        }
        return true;
    }

    std::string encode_text() const {
        try {
            std::stringstream ss;
            ss << std::fixed << std::setprecision(6);
            for (const auto& v : vertices) {
                ss << "v " << CGAL::to_double(v.x) << " " << CGAL::to_double(v.y) << " " << CGAL::to_double(v.z) << "\n";
            }
            if (!points.empty()) {
                ss << "p";
                for (int idx : points) ss << " " << idx;
                ss << "\n";
            }
            if (!segments.empty()) {
                ss << "s";
                for (const auto& seg : segments) {
                    ss << " " << seg[0] << " " << seg[1];
                }
                ss << "\n";
            }
            for (const auto& tri : triangles) {
                ss << "t " << tri[0] << " " << tri[1] << " " << tri[2] << "\n";
            }
            for (const auto& face : faces) {
                if (face.loops.empty()) continue;
                ss << "f";
                for (int idx : face.loops[0]) ss << " " << idx;
                ss << "\n";
                for (size_t i = 1; i < face.loops.size(); ++i) {
                    ss << "h";
                    for (int idx : face.loops[i]) ss << " " << idx;
                    ss << "\n";
                }
            }
            return ss.str();
        } catch (const std::exception& e) {
            std::cerr << "[Geometry] Error encoding text: " << e.what() << std::endl;
            return "";
        }
    }

    void decode_text(const std::string& text) {
        try {
            std::istringstream ss(text);
            std::string line;
            while (std::getline(ss, line)) {
                if (line.empty()) continue;
                std::istringstream ls(line);
                std::string code;
                if (!(ls >> code)) continue;

                if (code == "v") {
                    double x, y, z;
                    if (ls >> x >> y >> z) {
                        vertices.push_back({FT(x), FT(y), FT(z)});
                    }
                } else if (code == "f") {
                    Face f;
                    std::vector<int> loop;
                    int idx;
                    while (ls >> idx) loop.push_back(idx);
                    if (!loop.empty()) {
                        f.loops.push_back(loop);
                        faces.push_back(f);
                    }
                } else if (code == "h") {
                    if (!faces.empty()) {
                        std::vector<int> loop;
                        int idx;
                        while (ls >> idx) loop.push_back(idx);
                        if (!loop.empty()) faces.back().loops.push_back(loop);
                    }
                } else if (code == "s") {
                    int i1, i2;
                    while (ls >> i1 >> i2) segments.push_back({i1, i2});
                } else if (code == "p") {
                    int idx;
                    while (ls >> idx) points.push_back(idx);
                } else if (code == "t") {
                    int i1, i2, i3;
                    if (ls >> i1 >> i2 >> i3) triangles.push_back({i1, i2, i3});
                }
            }
        } catch (const std::exception& e) {
             std::cerr << "[Geometry] Error decoding text: " << e.what() << std::endl;
        }
    }
};

} // namespace geo
} // namespace jotcad
