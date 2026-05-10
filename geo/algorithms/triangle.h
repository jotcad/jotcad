#pragma once
#include "geometry.h"
#include <vector>
#include <cmath>

namespace jotcad {
namespace geo {

static void makeTriangle(Geometry& geo, double va, double vb, double vc) {
    // va is side along X, vb is side opposite first vertex, vc is side from origin
    // Vertex 0: (0,0)
    // Vertex 1: (va, 0)
    // Vertex 2: (x, y) using Law of Cosines
    double cosA = (va*va + vc*vc - vb*vb) / (2 * va * vc);
    double sinA = std::sqrt(std::max(0.0, 1.0 - cosA*cosA));
    
    FT v0x = 0, v0y = 0;
    FT v1x = FT(va), v1y = 0;
    FT v2x = FT(vc * cosA), v2y = FT(vc * sinA);

    // Centroid centering
    FT cx = (v0x + v1x + v2x) / FT(3);
    FT cy = (v0y + v1y + v2y) / FT(3);

    geo.vertices = {
        {v0x - cx, v0y - cy, FT(0)},
        {v1x - cx, v1y - cy, FT(0)},
        {v2x - cx, v2y - cy, FT(0)}
    };
    geo.faces = {
        {{{0, 1, 2}}}
    };
}

} // namespace geo
} // namespace jotcad
