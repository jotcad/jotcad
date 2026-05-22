#pragma once

#include <vector>
#include <map>
#include "../boolean/engine.h"
#include "../data/geometry.h"
#include "../math/matrix.h"

namespace jotcad {
namespace geo {
namespace unfold {

/**
 * Represents a single unfolded 2D patch (island).
 */
struct UnfoldPatch {
    // Original face indices from the source mesh
    std::vector<size_t> face_indices;
    
    // The 2D geometry of this patch
    Geometry geometry;
    
    // The coordinate system of this patch (world to local)
    Matrix tf;
    
    // Mapping from original edges to fold/cut tags
    // Key: pair of vertex indices (sorted), Value: tag ("fold" or "cut")
    std::map<std::pair<int, int>, std::string> edge_tags;

    // The 2D polygon set used for overlap detection during construction
    boolean::General_polygon_set_2 gps;
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
