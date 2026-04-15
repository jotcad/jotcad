#pragma once

#include <vector>
#include <string>
#include <array>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace jotcad {
namespace geo {

struct Vertex {
    double x, y, z;
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
        for (auto& v : vertices) {
            double x = v.x * tf[0] + v.y * tf[4] + v.z * tf[8] + tf[12];
            double y = v.x * tf[1] + v.y * tf[5] + v.z * tf[9] + tf[13];
            double z = v.x * tf[2] + v.y * tf[6] + v.z * tf[10] + tf[14];
            v.x = x; v.y = y; v.z = z;
        }
    }

    std::string encode_text() const {
        try {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(10);
            for (const auto& v : vertices) {
                ss << "v " << v.x << " " << v.y << " " << v.z 
                   << " " << v.x << " " << v.y << " " << v.z << "\n";
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
                        vertices.push_back({x1, y1, z1});
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
