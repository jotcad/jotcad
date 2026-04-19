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
    
    geo.vertices = {
        {FT(0), FT(0), FT(0)},
        {FT(va), FT(0), FT(0)},
        {FT(vc * cosA), FT(vc * sinA), FT(0)}
    };
    geo.faces = {
        {{{0, 1, 2}}}
    };
}

} // namespace geo
} // namespace jotcad
