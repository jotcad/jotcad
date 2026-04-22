#pragma once

namespace ruled_surfaces {

struct StoppingRuleStats {
  enum Reason {
    NOT_STOPPED,
    MAX_ITERATIONS_REACHED,
    CONVERGENCE_REACHED,
    TARGET_COST_REACHED
  };
  Reason reason = NOT_STOPPED;
  int iterations = 0;
  int last_improvement_iteration = 0;
};

// A simple stopping rule based on a fixed number of iterations.
struct MaxIterationsStoppingRule {
  explicit MaxIterationsStoppingRule(int max_iters,
                                     StoppingRuleStats* stats = nullptr)
      : max_iterations_(max_iters), stats_(stats) {}

  bool ShouldStop(int iteration, double /*best_cost*/,
                  int iters_without_improvement) const {
    if (iteration >= max_iterations_) {
      if (stats_) {
        stats_->reason = StoppingRuleStats::MAX_ITERATIONS_REACHED;
        stats_->iterations = iteration;
        stats_->last_improvement_iteration =
            iteration - iters_without_improvement;
      }
      return true;
    }
    return false;
  }

  int max_iterations() const { return max_iterations_; }

 private:
  const int max_iterations_;
  StoppingRuleStats* stats_ = nullptr;
};

// A stopping rule that terminates when a target cost is met, or after a
// maximum number of iterations.
struct TargetCostStoppingRule {
  TargetCostStoppingRule(double target, int max_iters,
                         StoppingRuleStats* stats = nullptr)
      : target_cost_(target), max_iterations_(max_iters), stats_(stats) {}

  bool ShouldStop(int iteration, double best_cost,
                  int iters_without_improvement) const {
    if (iteration >= max_iterations_) {
      if (stats_) {
        stats_->reason = StoppingRuleStats::MAX_ITERATIONS_REACHED;
        stats_->iterations = iteration;
        stats_->last_improvement_iteration =
            iteration - iters_without_improvement;
      }
      return true;
    }
    if (best_cost <= target_cost_) {
      if (stats_) {
        stats_->reason = StoppingRuleStats::TARGET_COST_REACHED;
        stats_->iterations = iteration;
        stats_->last_improvement_iteration =
            iteration - iters_without_improvement;
      }
      return true;
    }
    return false;
  }

  int max_iterations() const { return max_iterations_; }

 private:
  const double target_cost_;
  const int max_iterations_;
  StoppingRuleStats* stats_ = nullptr;
};

// A stopping rule that terminates when the solution stops improving for a
// given number of iterations, or after a maximum number of iterations.
// This rule is generally preferred over MaxIterationsStoppingRule as it
// terminates early if the solution converges.
struct ConvergenceStoppingRule {
  ConvergenceStoppingRule()
      : convergence_iterations_(100), max_iterations_(1000) {}
  ConvergenceStoppingRule(int convergence_iters, int max_iters,
                          StoppingRuleStats* stats = nullptr)
      : convergence_iterations_(convergence_iters),
        max_iterations_(max_iters),
        stats_(stats) {}

  bool ShouldStop(int iteration, double /*best_cost*/,
                  int iters_without_improvement) const {
    if (iteration >= max_iterations_) {
      if (stats_) {
        stats_->reason = StoppingRuleStats::MAX_ITERATIONS_REACHED;
        stats_->iterations = iteration;
        stats_->last_improvement_iteration =
            iteration - iters_without_improvement;
      }
      return true;
    }
    if (iters_without_improvement >= convergence_iterations_) {
      if (stats_) {
        stats_->reason = StoppingRuleStats::CONVERGENCE_REACHED;
        stats_->iterations = iteration;
        stats_->last_improvement_iteration =
            iteration - iters_without_improvement;
      }
      return true;
    }
    return false;
  }

  int max_iterations() const { return max_iterations_; }

 private:
  int convergence_iterations_;
  int max_iterations_;
  StoppingRuleStats* stats_ = nullptr;
};

}  // namespace ruled_surfaces
