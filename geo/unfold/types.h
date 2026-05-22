#pragma once

#include <vector>
#include <map>
#include <set>
#include <string>
#include "../boolean/engine.h"
#include "../math/matrix.h"

namespace jotcad {
namespace geo {
namespace unfold {

/**
 * Represents a single unfolded 2D patch (island).
 */
struct UnfoldPatch {
    // Original face indices from the source mesh that are part of this island
    std::vector<size_t> face_indices;

    // Boundary handles for the jigsaw merge.
    // Key: Halfedge index (source -> target) in original 3D mesh.
    // Value: Directed 2D segment (PointU, PointV) in this patch's local space.
    std::map<size_t, std::pair<EK::Point_2, EK::Point_2>> boundary_halfedges;

    // The 2D polygon set of the entire island (for overlap checks)
    boolean::General_polygon_set_2 gps;

    // The final geometry (vertices and loops) in 2D
    Geometry geometry;
    
    // Base transform for the entire island (from world 3D to island 2D root)
    Matrix tf;
    
    // Tags for edges within this island ("fold" or "cut")
    // Key: pair of vertex indices (sorted), Value: tag ("fold" or "cut")
    std::map<std::pair<int, int>, std::string> edge_tags;

    // Explicit 2D segments for fold lines
    std::vector<std::pair<EK::Point_2, EK::Point_2>> fold_segments;
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
