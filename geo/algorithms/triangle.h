#pragma once
#include "geometry.h"
#include <vector>
#include <cmath>
#include <optional>

namespace jotcad {
namespace geo {

static void makeTriangle(Geometry& geo, 
                         std::optional<double> a_opt, 
                         std::optional<double> b_opt, 
                         std::optional<double> c_opt, 
                         std::optional<double> x_opt, 
                         std::optional<double> y_opt, 
                         std::optional<bool> center_opt) {
    bool has_x = x_opt.has_value();
    bool has_y = y_opt.has_value();
    bool has_a = a_opt.has_value();
    bool has_b = b_opt.has_value();
    bool has_c = c_opt.has_value();

    FT v0x = 0, v0y = 0;
    FT v1x = 0, v1y = 0;
    FT v2x = 0, v2y = 0;

    // Case 1: 2 orthogonal legs (x and y, or just a and b with no c)
    if ((has_x && has_y && !has_c) || (has_a && has_b && !has_c && !has_x && !has_y)) {
        double x_val = has_x ? x_opt.value() : a_opt.value();
        double y_val = has_y ? y_opt.value() : b_opt.value();
        v0x = 0; v0y = 0;
        if (x_val >= 0) {
            v1x = FT(x_val); v1y = FT(y_val);
            v2x = 0; v2y = FT(y_val);
        } else {
            v1x = 0; v1y = FT(y_val);
            v2x = FT(x_val); v2y = FT(y_val);
        }
    } 
    // Case 2: y-axis aligned 3-side triangle
    else if (has_y && (has_b || has_a)) {
        double va = y_opt.value();
        double vb = has_a ? b_opt.value_or(0.0) : a_opt.value_or(b_opt.value_or(0.0));
        double vc = has_c ? c_opt.value() : b_opt.value_or(0.0);

        double denom = 2.0 * va * vc;
        double cosA = (denom > 1e-9) ? ((va*va + vc*vc - vb*vb) / denom) : 0.0;
        double sinA = std::sqrt(std::max(0.0, 1.0 - cosA*cosA));

        v0x = 0; v0y = 0;
        v1x = FT(vc * sinA); v1y = FT(vc * cosA);
        v2x = 0; v2y = FT(va);
    }
    // Case 3: x-axis aligned 3-side triangle (default)
    else {
        double va = has_x ? x_opt.value() : a_opt.value_or(0.0);
        double vb = b_opt.value_or(0.0);
        double vc = c_opt.value_or(0.0);

        double denom = 2.0 * va * vc;
        double cosA = (denom > 1e-9) ? ((va*va + vc*vc - vb*vb) / denom) : 0.0;
        double sinA = std::sqrt(std::max(0.0, 1.0 - cosA*cosA));

        v0x = 0; v0y = 0;
        v1x = FT(va); v1y = 0;
        v2x = FT(vc * cosA); v2y = FT(vc * sinA);
    }

    bool center = center_opt.value_or(has_c && !has_x && !has_y);
    FT cx = center ? (v0x + v1x + v2x) / FT(3) : FT(0);
    FT cy = center ? (v0y + v1y + v2y) / FT(3) : FT(0);

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
