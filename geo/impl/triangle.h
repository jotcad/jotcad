#pragma once
#include "geometry.h"
#include <cmath>
#include <vector>

namespace jotcad {
namespace geo {

static void makeTriangle(Geometry& geo, double a, double b, double c) {
    geo.vertices.push_back({0, 0, 0});
    geo.vertices.push_back({a, 0, 0});
    
    // Using law of cosines to find the third vertex
    double cosA = (a*a + c*c - b*b) / (2 * a * c);
    double sinA = std::sqrt(std::max(0.0, 1 - cosA*cosA));
    
    geo.vertices.push_back({c * cosA, c * sinA, 0});
    geo.faces.push_back({{{0, 1, 2}}});
}

} // namespace geo
} // namespace jotcad
