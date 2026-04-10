#pragma once
#include "geometry.h"
#include <vector>

namespace jotcad {
namespace geo {

static void applyOutline(Geometry& geo) {
    for (const auto& face : geo.faces) {
        for (const auto& loop : face.loops) {
            if (loop.empty()) continue;
            for (size_t i = 0; i < loop.size(); ++i) {
                geo.segments.push_back({loop[i], loop[(i + 1) % loop.size()]});
            }
        }
    }
    geo.faces.clear();
}

} // namespace geo
} // namespace jotcad
