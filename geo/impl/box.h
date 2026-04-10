#pragma once
#include "geometry.h"
#include <vector>

namespace jotcad {
namespace geo {

static void makeBox(Geometry& geo, double w, double h, double d) {
    geo.vertices = {
        {0, 0, 0}, {w, 0, 0}, {w, h, 0}, {0, h, 0},
        {0, 0, d}, {w, 0, d}, {w, h, d}, {0, h, d}
    };

    geo.faces = {
        {{{0, 1, 2, 3}}}, // Bottom
        {{{4, 5, 6, 7}}}, // Top
        {{{0, 1, 5, 4}}}, // Front
        {{{1, 2, 6, 5}}}, // Right
        {{{2, 3, 7, 6}}}, // Back
        {{{3, 0, 4, 7}}}  // Left
    };
}

} // namespace geo
} // namespace jotcad
