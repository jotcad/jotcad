#pragma once
#include "geometry.h"
#include <cmath>
#include <string>

namespace jotcad {
namespace geo {

static void makeHexagon(Geometry& geo, double radius, const std::string& variant) {
    double s3 = std::sqrt(3.0);
    if (variant == "middle") {
        geo.vertices.push_back({  radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ -radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ -radius/2.0, -radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({  radius/2.0, -radius * s3 / 2.0, 0.0 });
    } else if (variant == "cap") {
        geo.vertices.push_back({ radius, 0.0, 0.0 });
        geo.vertices.push_back({ radius/2.0,  radius * s3 / 2.0, 0.0 });
        geo.vertices.push_back({ radius/2.0, -radius * s3 / 2.0, 0.0 });
    } else if (variant == "sector") {
        geo.vertices.push_back({ 0.0, 0.0, 0.0 });
        geo.vertices.push_back({ radius, 0.0, 0.0 });
        geo.vertices.push_back({ radius / 2.0, radius * s3 / 2.0, 0.0 });
    } else if (variant == "half") {
        geo.vertices.push_back({  radius, 0.0, 0.0 }); 
        geo.vertices.push_back({  radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
        geo.vertices.push_back({ -radius / 2.0,  radius * s3 / 2.0, 0.0 }); 
        geo.vertices.push_back({ -radius, 0.0, 0.0 }); 
    } else {
        for (int i = 0; i < 6; ++i) {
            double angle_rad = (M_PI / 180.0) * (60.0 * i);
            geo.vertices.push_back({ radius * std::cos(angle_rad), radius * std::sin(angle_rad), 0.0 });
        }
    }

    Geometry::Face face;
    std::vector<int> loop;
    for (size_t i = 0; i < geo.vertices.size(); ++i) loop.push_back(i);
    face.loops.push_back(loop);
    geo.faces.push_back(face);
}

} // namespace geo
} // namespace jotcad
