#pragma once

#include <limits>
#include <vector>

#include "solution_stats.h"
#include "types.h"

namespace ruled_surfaces {

class RuledSurfaceVisitor {
 public:
  enum VisitControl { kContinue, kStop };
  virtual ~RuledSurfaceVisitor() = default;

  // Called when a seam-search strategy evaluates a shift.
  virtual VisitControl OnSeam(bool is_reversed) { return kContinue; }
  virtual VisitControl OnStart(const PolygonalChain& p,
                               const PolygonalChain& q) {
    return kContinue;
  }
  virtual VisitControl OnValidPolylines(const PolygonalChain& p,
                                        const PolygonalChain& q) {
    return kContinue;
  }
<<<<<<< HEAD
  virtual VisitControl OnMesh(const Mesh& mesh, double cost) {
    return kContinue;
  }
=======
  virtual VisitControl OnMesh(const Mesh& mesh, double cost) { return kContinue; }
>>>>>>> main
  virtual VisitControl OnPermutation(const Mesh& mesh) { return kContinue; }
  virtual VisitControl OnValidMesh(const Mesh& mesh, double cost) {
    return kContinue;
  }
  virtual VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) {
    return OnValidMesh(mesh, cost);
  }
  virtual VisitControl OnValidMesh(const Mesh& mesh, double cost,
                                   const std::vector<bool>& moves) {
    return OnValidMesh(mesh, cost);
  }
  virtual VisitControl OnInvalid(const Mesh& mesh) { return kContinue; }
  virtual void OnSearchStep(bool is_seam_move, double temp,
                            double seam_move_prob) {}
  virtual void OnSeamMove(int next_shift) {}
  virtual void OnAcceptance(bool accepted, double new_cost, double current_cost,
                            bool was_seam_move) {}
  virtual void OnFinish(const SolutionStats& stats) {}

 protected:
  RuledSurfaceVisitor() = default;
};

// BestSeamSearchVisitor is a RuledSurfaceVisitor that keeps track of the
// best solution found during a seam search. It records the best mesh, its cost,
// and the corresponding seam parameters (shift and reversal).
class BestSeamSearchVisitor : public RuledSurfaceVisitor {
 public:
  BestSeamSearchVisitor(Mesh* mesh, SolutionStats* stats)
      : mesh_(mesh),
        stats_(stats),
        best_cost_(std::numeric_limits<double>::infinity()),
        current_shift_(-1),
        current_is_reversed_(false) {
    if (stats_ != nullptr) {
      stats_->cost = std::numeric_limits<double>::infinity();
      stats_->shift = -1;
      stats_->is_reversed = false;
    }
  }

  VisitControl OnStart(const PolygonalChain& p,
                       const PolygonalChain& q) override {
    // Reset internal state for subsequent runs with the same visitor instance.
    best_cost_ = std::numeric_limits<double>::infinity();
    current_shift_ = -1;
    current_is_reversed_ = false;
    return RuledSurfaceVisitor::OnStart(p, q);
  }

  VisitControl OnSeam(bool is_reversed) override {
    current_is_reversed_ = is_reversed;
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    if (cost < best_cost_) {
      best_cost_ = cost;
      if (mesh_ != nullptr) {
        *mesh_ = mesh;
      }
      if (stats_ != nullptr) {
        stats_->shift = current_shift_;
        stats_->is_reversed = current_is_reversed_;
      }
    }
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) override {
    current_shift_ = shift;
    return OnValidMesh(mesh, cost);
  }

  void OnFinish(const SolutionStats& final_strategy_stats) override {
    if (stats_ != nullptr) {
      stats_->status = (best_cost_ == std::numeric_limits<double>::infinity())
                           ? SolutionStats::NO_SOLUTION_FOUND
                           : SolutionStats::OK;
      stats_->cost = best_cost_;
      stats_->paths_evaluated = final_strategy_stats.paths_evaluated;
    }
  }

  double best_cost() const { return best_cost_; }

 protected:
  Mesh* mesh_ = nullptr;
  SolutionStats* stats_ = nullptr;
  double best_cost_ = std::numeric_limits<double>::infinity();
  int current_shift_ = -1;
  bool current_is_reversed_ = false;
};

// BestTriangulationSearchSolutionVisitor is a RuledSurfaceVisitor designed for
// triangulation search algorithms. It tracks the best mesh found based on cost,
// without considering seam-related parameters like shift or reversal.
class BestTriangulationSearchSolutionVisitor : public RuledSurfaceVisitor {
 public:
  struct Solution {
    Mesh mesh;
    int shift;
    bool is_reversed;
    int p_moves = 0;
    std::vector<bool> moves;
  };

  BestTriangulationSearchSolutionVisitor(Mesh* mesh, SolutionStats* stats)
      : mesh_(mesh),
        stats_(stats),
        best_cost_(std::numeric_limits<double>::infinity()),
        current_shift_(-1),
        current_is_reversed_(false) {
    if (stats_ != nullptr) {
      stats_->cost = std::numeric_limits<double>::infinity();
    }
  }

  VisitControl OnStart(const PolygonalChain& p,
                       const PolygonalChain& q) override {
    return RuledSurfaceVisitor::OnStart(p, q);
  }

  VisitControl OnSeam(bool is_reversed) override {
    current_is_reversed_ = is_reversed;
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    // This is called by legacy triangulation algorithms that do not pass moves.
    // We treat it as 0 moves for tie-breaking.
    if (best_solutions_.empty() || cost < best_cost_ - kEpsilon) {
      best_cost_ = cost;
      best_solutions_.clear();
      best_solutions_.push_back(
          {mesh, current_shift_, current_is_reversed_, 0, {}});
      if (mesh_) *mesh_ = mesh;
      if (stats_) {
        stats_->cost = cost;
        stats_->shift = current_shift_;
        stats_->is_reversed = current_is_reversed_;
      }
    } else if (cost <= best_cost_ + kEpsilon) {
      // Tie-breaking by number of moves (more moves is better)
      if (0 > best_solutions_[0].p_moves) {
        best_solutions_.clear();
        best_solutions_.push_back(
            {mesh, current_shift_, current_is_reversed_, 0, {}});
        if (mesh_) *mesh_ = mesh;
        if (stats_) {
          stats_->cost = cost;
          stats_->shift = current_shift_;
          stats_->is_reversed = current_is_reversed_;
        }
      } else if (0 == best_solutions_[0].p_moves) {
        best_solutions_.push_back(
            {mesh, current_shift_, current_is_reversed_, 0, {}});
      }
    }
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost,
                           const std::vector<bool>& moves) override {
    int p_moves = 0;
    for (bool move : moves) {
      if (move) p_moves++;
    }

    if (best_solutions_.empty() || cost < best_cost_ - kEpsilon) {
      best_cost_ = cost;
      best_solutions_.clear();
      best_solutions_.push_back(
          {mesh, current_shift_, current_is_reversed_, p_moves, moves});
      if (mesh_) *mesh_ = mesh;
      if (stats_) {
        stats_->cost = cost;
        stats_->shift = current_shift_;
        stats_->is_reversed = current_is_reversed_;
      }
    } else if (cost <= best_cost_ + kEpsilon) {
      if (p_moves > best_solutions_[0].p_moves) {
        best_cost_ = cost;
        best_solutions_.clear();
        best_solutions_.push_back(
            {mesh, current_shift_, current_is_reversed_, p_moves, moves});
        if (mesh_) *mesh_ = mesh;
        if (stats_) {
          stats_->cost = cost;
          stats_->shift = current_shift_;
          stats_->is_reversed = current_is_reversed_;
        }
      } else if (p_moves == best_solutions_[0].p_moves) {
        best_solutions_.push_back(
            {mesh, current_shift_, current_is_reversed_, p_moves, moves});
      }
    }
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) override {
    current_shift_ = shift;
    return OnValidMesh(mesh, cost);
  }

  void OnFinish(const SolutionStats& final_strategy_stats) override {
    if (stats_ != nullptr) {
<<<<<<< HEAD
      stats_->status = best_solutions_.empty()
                           ? SolutionStats::NO_SOLUTION_FOUND
                           : SolutionStats::OK;
=======
      stats_->status = best_solutions_.empty() ? SolutionStats::NO_SOLUTION_FOUND
                                               : SolutionStats::OK;
>>>>>>> main
      stats_->cost = best_cost_;
      stats_->paths_evaluated = final_strategy_stats.paths_evaluated;
    }
  }

  double best_cost() const { return best_cost_; }

  const std::vector<Solution>& best_solutions() const {
    return best_solutions_;
  }

  bool is_ambiguous() const { return best_solutions_.size() > 1; }

 protected:
  Mesh* mesh_ = nullptr;
  // Non-owning pointer to external stats struct.
  SolutionStats* stats_ = nullptr;
  double best_cost_ = std::numeric_limits<double>::infinity();
  int current_shift_ = -1;
  bool current_is_reversed_ = false;
  std::vector<Solution> best_solutions_;
};

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces

>>>>>>> main
