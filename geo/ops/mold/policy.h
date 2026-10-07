#pragma once

#include "boolean/engine.h"

namespace jotcad {
namespace geo {
namespace mold {

typedef EK::FT FT;

/**
 * @brief Governs how faces parallel to draw direction (n * d == 0) are handled in candidate coverage.
 * 
 * Note on Search Semantics: In the beam search frontier, marking a face as "handled" represents
 * an assertion of kinematic demoldability—certifying that the face is releasable along a direction
 * already present in the chain without backdraft. It does not mandate that an intermediate piece's
 * extruded envelope wedge physically presses against it; any unshadowed parallel faces remaining on the
 * residual stock are physically formed by the terminal piece (P_K = B_{K-1}) and validated for demoldability.
 */
enum class VerticalWallPolicy {
    EXCLUDE,          ///< Baseline/current: area_2d > 0 only; vertical walls are excluded from envelope and coverage
    UNSHADOWED_ONLY,  ///< Include vertical walls whose 2D envelope footprint is unoccluded by higher geometry
    ALL_IN_PATCH      ///< Legacy da0a885: adopt all vertical walls belonging to the connected patch component
};

/**
 * @brief Governs the vertical reach of the synthesized solid wedge sidewall skirt.
 */
enum class WedgeSkirtPolicy {
    CEILING_ONLY,        ///< Baseline/current: sidewalls sweep from envelope surface strictly upward to ceiling
    DROP_TO_WALL_BOTTOM  ///< Sidewalls drop downward along unshadowed vertical walls to their bottom edge
};

/**
 * @brief Governs how remaining cavity coverage is certified.
 */
enum class CoverageCertificationPolicy {
    BOOKKEEPING_ONLY,       ///< Baseline/current: unhandled_area <= 0 via discrete face area subtraction
    RESIDUAL_STOCK_CONTACT  ///< Approach B: verify remaining unhandled area via physical contact with residual stock
};

/**
 * @brief Modular policy class encapsulating algorithm variants for mold beam search and wedge synthesis.
 */
struct MoldDecompositionPolicy {
    VerticalWallPolicy vertical_walls = VerticalWallPolicy::EXCLUDE;
    FT vertical_wall_weight = FT(1);
    WedgeSkirtPolicy wedge_skirt = WedgeSkirtPolicy::CEILING_ONLY;
    bool enable_terminal_closure = true;
    CoverageCertificationPolicy certification = CoverageCertificationPolicy::BOOKKEEPING_ONLY;

    /// @brief Baseline matching the current codebase configuration
    static MoldDecompositionPolicy current() {
        MoldDecompositionPolicy p;
        p.vertical_walls = VerticalWallPolicy::EXCLUDE;
        p.vertical_wall_weight = FT(1);
        p.wedge_skirt = WedgeSkirtPolicy::CEILING_ONLY;
        p.enable_terminal_closure = true;
        p.certification = CoverageCertificationPolicy::BOOKKEEPING_ONLY;
        return p;
    }

    /// @brief Legacy patch configuration (commit da0a885 / 7160ab1 behavior)
    static MoldDecompositionPolicy legacy_patch() {
        MoldDecompositionPolicy p;
        p.vertical_walls = VerticalWallPolicy::ALL_IN_PATCH;
        p.vertical_wall_weight = FT(1);
        p.wedge_skirt = WedgeSkirtPolicy::CEILING_ONLY;
        p.enable_terminal_closure = true;
        p.certification = CoverageCertificationPolicy::BOOKKEEPING_ONLY;
        return p;
    }

    /// @brief Certified unshadowed envelope with physical residual stock contact verification
    static MoldDecompositionPolicy analytical_envelope() {
        MoldDecompositionPolicy p;
        p.vertical_walls = VerticalWallPolicy::UNSHADOWED_ONLY;
        p.vertical_wall_weight = FT(1);
        p.wedge_skirt = WedgeSkirtPolicy::DROP_TO_WALL_BOTTOM;
        p.enable_terminal_closure = true;
        p.certification = CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT;
        return p;
    }
};

} // namespace mold
} // namespace geo
} // namespace jotcad
