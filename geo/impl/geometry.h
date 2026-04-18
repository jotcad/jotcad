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

struct Geometry {
    std::vector<Vertex> vertices;
    std::vector<int> points;
    std::vector<std::array<int, 2>> segments;
    std::vector<std::array<int, 3>> triangles;
    
    struct Face {
        std::vector<std::vector<int>> loops;
    };
    std::vector<Face> faces;

    void apply_tf(const std::vector<double>& tf) {
        if (tf.size() < 16) return;
        // Construct exact transformation
        Transformation t(tf[0], tf[1], tf[2], tf[3],
                         tf[4], tf[5], tf[6], tf[7],
                         tf[8], tf[9], tf[10], tf[11]);
        
        for (auto& v : vertices) {
            Point_3 p(v.x, v.y, v.z);
            p = t.transform(p);
            v.x = p.x(); v.y = p.y(); v.z = p.z();
        }
    }

    std::string encode_text() const {
        try {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(18);
            for (const auto& v : vertices) {
                double dx = CGAL::to_double(v.x);
                double dy = CGAL::to_double(v.y);
                double dz = CGAL::to_double(v.z);
                ss << "v " << dx << " " << dy << " " << dz 
                   << " " << dx << " " << dy << " " << dz << "\n";
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
                    double x1, y1, z1;
                    if (ls >> x1 >> y1 >> z1) {
                        vertices.push_back({FT(x1), FT(y1), FT(z1)});
                    }
                } else if (code == "p") {
                    int idx;
                    while (ls >> idx) points.push_back(idx);
                } else if (code == "s") {
                    int v1, v2;
                    while (ls >> v1 >> v2) segments.push_back({v1, v2});
                } else if (code == "t") {
                    int v1, v2, v3;
                    while (ls >> v1 >> v2 >> v3) triangles.push_back({v1, v2, v3});
                } else if (code == "f") {
                    Face face;
                    std::vector<int> loop;
                    int idx;
                    while (ls >> idx) loop.push_back(idx);
                    if (!loop.empty()) {
                        face.loops.push_back(std::move(loop));
                        faces.push_back(std::move(face));
                    }
                } else if (code == "h") {
                    if (faces.empty()) continue;
                    std::vector<int> hole;
                    int idx;
                    while (ls >> idx) hole.push_back(idx);
                    if (!hole.empty()) {
                        faces.back().loops.push_back(std::move(hole));
                    }
                }
            }
        } catch (const std::exception& e) {
             std::cerr << "[Geometry] Error decoding text: " << e.what() << std::endl;
        }
    }
};

} // namespace geo
} // namespace jotcad
