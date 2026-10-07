#pragma once

#include "types.h"
#include <vector>
#include <string>
#include <memory>
#include <iostream>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Abstract progress and score snapshot passed to visitors and termination policies.
 * 
 * Fully decoupled from candidate search node representations.
 */
struct SearchProgress {
    size_t iteration = 0;
    size_t validations_count = 0;
    size_t frontier_size = 0;

    // Status of best complete solution found so far
    bool found_complete = false;
    size_t best_complete_pieces = 0;
    FT best_complete_energy = FT(-1);

    // Current step & frontier metrics
    size_t current_pieces = 0;
    FT current_energy = FT(0);
    FT min_frontier_energy = FT(0);
};

/**
 * @brief Abstract visitor interface for monitoring and governing search lifecycle and termination.
 */
class SearchVisitor {
public:
    virtual ~SearchVisitor() = default;

    /// Called at the start of each search iteration when a candidate is popped.
    virtual void on_iteration_start(const SearchProgress& progress) {}

    /// Called immediately when a certified complete decomposition is found or improved.
    virtual void on_complete_found(const SearchProgress& progress) {}

    /// Called after child expansion and frontier re-sorting.
    virtual void on_frontier_updated(const SearchProgress& progress) {}

    /// Evaluates whether the search should terminate early according to policy.
    virtual bool should_terminate(const SearchProgress& progress, std::string* stop_reason = nullptr) {
        return false;
    }
};

/**
 * @brief Configuration parameters for convergence tracking stop rule.
 */
struct ConvergenceParams {
    /// If true, stop immediately when a complete solution achieving theoretical lower bound (K <= target_min_pieces) is found.
    bool stop_on_minimal_pieces = true;
    size_t target_min_pieces = 2; // Physical lower bound for any closed 3D solid mold

    /// Maximum iterations without improvement in complete solution energy after the first complete solution is found.
    size_t stagnation_iterations = 20;

    /// Hard cap on total search iterations.
    size_t max_iterations = 200;

    /// Hard cap on expensive 3D CSG stock carving validations.
    size_t max_validations = 100;
};

/**
 * @brief Convergence tracking stop-rule class that manages search termination policy.
 * 
 * Replaces ad-hoc queue pruning with principled convergence detection:
 * 1. Theoretical Lower Bound: Instant termination on certified 2-piece complete solution.
 * 2. Score Convergence / Plateau: Terminate if no better complete solution is found after N iterations.
 * 3. Budget Protection: Caps on iteration count and CSG validations.
 */
class ConvergenceStopRule : public SearchVisitor {
private:
    ConvergenceParams params_;
    size_t stagnant_count_ = 0;
    FT best_energy_ = FT(-1);
    bool had_complete_ = false;

public:
    explicit ConvergenceStopRule(const ConvergenceParams& params = ConvergenceParams())
        : params_(params) {}

    void on_iteration_start(const SearchProgress& progress) override {
        // Status tracking
    }

    void on_complete_found(const SearchProgress& progress) override {
        if (!had_complete_ || progress.best_complete_energy < best_energy_) {
            best_energy_ = progress.best_complete_energy;
            stagnant_count_ = 0;
            had_complete_ = true;
        }
    }

    void on_frontier_updated(const SearchProgress& progress) override {
        if (had_complete_) {
            if (progress.found_complete && progress.best_complete_energy < best_energy_) {
                best_energy_ = progress.best_complete_energy;
                stagnant_count_ = 0;
            } else {
                stagnant_count_++;
            }
        }
    }

    bool should_terminate(const SearchProgress& progress, std::string* stop_reason = nullptr) override {
        // 1. Budget checks
        if (progress.iteration >= params_.max_iterations) {
            if (stop_reason) {
                *stop_reason = "Max iteration budget reached (" + std::to_string(params_.max_iterations) + ")";
            }
            return true;
        }

        if (progress.validations_count >= params_.max_validations) {
            if (stop_reason) {
                *stop_reason = "Max CSG validation budget reached (" + std::to_string(params_.max_validations) + ")";
            }
            return true;
        }

        // 2. Physical lower bound convergence: 2-piece airtight mold can never be improved in piece count
        if (params_.stop_on_minimal_pieces && progress.found_complete) {
            if (progress.best_complete_pieces <= params_.target_min_pieces) {
                if (stop_reason) {
                    *stop_reason = "Converged to theoretical lower bound (" + 
                                   std::to_string(progress.best_complete_pieces) + " pieces)";
                }
                return true;
            }
        }

        // 3. Score plateau convergence: complete solution found, but no improvement for N iterations
        if (had_complete_ && stagnant_count_ >= params_.stagnation_iterations) {
            if (stop_reason) {
                *stop_reason = "Score converged: no improvement for " + 
                               std::to_string(stagnant_count_) + " iterations (best energy = " +
                               std::to_string((int)CGAL::to_double(best_energy_)) + " mm²)";
            }
            return true;
        }

        return false;
    }

    size_t stagnant_count() const { return stagnant_count_; }
    FT best_energy() const { return best_energy_; }
    const ConvergenceParams& params() const { return params_; }
};

} // namespace mold
} // namespace geo
} // namespace jotcad
