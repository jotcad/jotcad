#pragma once
#include "protocols.h"
#include <cmath>
#include <algorithm>

namespace jotcad {
namespace geo {

inline int zag(double diameter, double tolerance) {
    if (diameter <= 0) return 3;
    double radius = diameter / 2.0;
    double k = tolerance / radius;
    int sides = (int)std::ceil(M_PI / std::sqrt(k * 2.0));
    return std::max(sides, 3);
}

} // namespace geo
} // namespace jotcad
