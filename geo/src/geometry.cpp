#include "../include/geometry.h"
#include <sstream>
#include <iomanip>

namespace jotcad {
namespace geo {

std::string Geometry::encode_text() const {
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
        
        // Outer boundary
        ss << "f";
        for (int idx : face.loops[0]) ss << " " << idx;
        ss << "\n";

        // Holes
        for (size_t i = 1; i < face.loops.size(); ++i) {
            ss << "h";
            for (int idx : face.loops[i]) ss << " " << idx;
            ss << "\n";
        }
    }

    return ss.str();
}

Geometry Geometry::decode_text(const std::string& text) {
    Geometry geo;
    std::istringstream ss(text);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string code;
        ls >> code;

        if (code == "v") {
            double x1, y1, z1, x2, y2, z2;
            while (ls >> x1 >> y1 >> z1 >> x2 >> y2 >> z2) {
                // Following JS logic: skip first 3, take next 3
                geo.vertices.push_back({x2, y2, z2});
            }
        } else if (code == "p") {
            int idx;
            while (ls >> idx) geo.points.push_back(idx);
        } else if (code == "s") {
            int v1, v2;
            while (ls >> v1 >> v2) {
                geo.segments.push_back({v1, v2});
            }
        } else if (code == "t") {
            int v1, v2, v3;
            if (ls >> v1 >> v2 >> v3) {
                geo.triangles.push_back({v1, v2, v3});
            }
        } else if (code == "f") {
            Geometry::Face face;
            std::vector<int> loop;
            int idx;
            while (ls >> idx) loop.push_back(idx);
            face.loops.push_back(std::move(loop));
            geo.faces.push_back(std::move(face));
        } else if (code == "h") {
            if (geo.faces.empty()) continue; // Should not happen in valid .jot
            std::vector<int> hole;
            int idx;
            while (ls >> idx) hole.push_back(idx);
            geo.faces.back().loops.push_back(std::move(hole));
        }
    }

    return geo;
}

} // namespace geo
} // namespace jotcad
