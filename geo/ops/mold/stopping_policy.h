#pragma once

#include "boolean/engine.h"

namespace jotcad {
namespace geo {
namespace mold {

typedef EK::FT FT;

/**
 * @brief Search lifecycle and termination policy governing search budgets and convergence criteria.
 * 
 * Completely decoupled from physical and geometric decomposition policies.
 */
struct MoldStoppingPolicy {
    /// If true, stop immediately when a complete solution achieving theoretical lower bound (K <= target_min_pieces) is found.
    bool stop_on_minimal_pieces = true;
    size_t target_min_pieces = 2; // Physical lower bound for any closed 3D solid mold

    /// Maximum iterations without improvement in complete solution energy after the first complete solution is found.
    size_t stagnation_iterations = 20;

    /// Hard cap on total search iterations.
    size_t max_iterations = 200;

    /// Hard cap on expensive 3D CSG stock carving validations.
    size_t max_validations = 100;

    /// Minimum energy improvement in mm^2 to reset stagnation counter.
    FT min_energy_improvement = FT(1) / FT(100);

    /// @brief Default balanced search budget
    static MoldStoppingPolicy standard() {
        return MoldStoppingPolicy();
    }

    /// @brief Fast interactive or preview search budget with aggressive early exit
    static MoldStoppingPolicy quick_preview() {
        MoldStoppingPolicy p;
        p.max_iterations = 30;
        p.max_validations = 15;
        p.stagnation_iterations = 5;
        return p;
    }

    /// @brief Deep exhaustive search for complex multi-piece geometries
    static MoldStoppingPolicy exhaustive() {
        MoldStoppingPolicy p;
        p.max_iterations = 1000;
        p.max_validations = 500;
        p.stagnation_iterations = 50;
        return p;
    }
};

using ConvergenceParams = MoldStoppingPolicy;

} // namespace mold
} // namespace geo
} // namespace jotcad
