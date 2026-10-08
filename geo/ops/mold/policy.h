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
    CoverageCertificationPolicy certification = CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT;

    // Demoldability & Physical Shrinkage Tolerances
    FT min_backdraft_area_threshold = FT(1) / FT(2); ///< 0.5 mm^2 physical shrinkage allowance for micro-undercuts
    bool ignore_boundary_seams = true;              ///< Treats parting boundary slivers (1e-3 <= d < 1e-2 mm) as flash
    FT cavity_contact_distance_sq = FT(1) / FT(10000); ///< 1e-4 mm^2 (10 um) cavity contact proximity
    FT boundary_seam_distance_sq = FT(1) / FT(1000000); ///< 1e-6 mm^2 (1 um) exact part face vs seam boundary
    FT zero_draft_dot_epsilon = FT(1) / FT(1000000);   ///< 1e-6 zero draft planar margin

    // Kinematics & Candidate Filtering
    FT acute_cone_dot_sq_ratio = FT(8) / FT(10);        ///< 0.8 (< 26.5 deg acute cone check)
    FT coplanar_sliding_sin_sq_cutoff = FT(1) / FT(10000); ///< 1e-4 (< 0.57 deg coplanar cross product cutoff)

    // Patch Clustering & Deduplication
    FT jaccard_threshold = FT(85) / FT(100);            ///< 0.85 Jaccard virgin patch overlap threshold

    // Search Weights & Energy Optimization
    FT lambda_pieces = FT(50);                          ///< 50 mm^2 piece complexity regularizer
    FT unhandled_area_zero_epsilon = FT(1) / FT(1000);  ///< 0.001 mm^2 residual cavity zeroing threshold
    size_t frontier_bound = 50;                         ///< Max priority queue capacity
    size_t search_threads = 0;                          ///< 0 = automatic (std::thread::hardware_concurrency()), >0 = fixed worker count

    // Mesh Synthesis Tolerances
    FT pinch_bridge_width = FT(1) / FT(100);            ///< 0.01 mm bridge for 2D boundary polygon pinch points

    /// @brief Baseline matching the certified analytical configuration
    static MoldDecompositionPolicy current() {
        MoldDecompositionPolicy p;
        p.vertical_walls = VerticalWallPolicy::EXCLUDE;
        p.vertical_wall_weight = FT(1);
        p.wedge_skirt = WedgeSkirtPolicy::CEILING_ONLY;
        p.enable_terminal_closure = true;
        p.certification = CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT;
        p.min_backdraft_area_threshold = FT(1) / FT(2);
        p.ignore_boundary_seams = true;
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
        p.min_backdraft_area_threshold = FT(1) / FT(100);
        p.ignore_boundary_seams = false;
        return p;
    }

    /// @brief Certified unshadowed envelope with physical residual stock demoldability closure
    static MoldDecompositionPolicy analytical_envelope() {
        MoldDecompositionPolicy p;
        p.vertical_walls = VerticalWallPolicy::EXCLUDE;
        p.vertical_wall_weight = FT(1);
        p.wedge_skirt = WedgeSkirtPolicy::CEILING_ONLY;
        p.enable_terminal_closure = true;
        p.certification = CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT;
        p.min_backdraft_area_threshold = FT(1) / FT(2);
        p.ignore_boundary_seams = true;
        return p;
    }
};

} // namespace mold
} // namespace geo
} // namespace jotcad
