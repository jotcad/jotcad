#pragma once

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_test_utils.h"
#include "visitor.h"

namespace ruled_surfaces {

class SeamSearchVerboseVisitor : public RuledSurfaceVisitor {
 public:
  explicit SeamSearchVerboseVisitor(std::ostream* out = &std::cout)
      : out_(out) {}

  VisitControl OnSeam(bool is_reversed) override {
    current_is_reversed_ = is_reversed;
    *out_ << "    Seam: " << (is_reversed ? "reversed" : "normal") << "\n";
    return kContinue;
  }

  VisitControl OnStart(const PolygonalChain&, const PolygonalChain&) override {
    *out_ << "--- Search Started ---\n";
    iteration_count_ = 0;
    invalid_count_ = 0;
    best_cost_ = std::numeric_limits<double>::infinity();
    return kContinue;
  }

  VisitControl OnInvalid(const Mesh& mesh) override {
    invalid_count_++;
    *out_ << "    Invalid solution #" << invalid_count_ << " found.\n";
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    // This overload is called by strategies that are not shift-aware.
    // The current_shift_ will be whatever was last set by OnShift.
    return OnValidMesh(mesh, cost, current_shift_);
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) override {
    // This is now only called for candidate solutions before acceptance.
    candidate_cost_ = cost;
    candidate_shift_ = shift;
    return kContinue;
  }

  void OnSearchStep(bool is_seam_move, double temp,
                    double seam_move_prob) override {
    const char* move_type = is_seam_move ? "seam" : "triangulation";
    *out_ << "Iteration " << std::setw(4) << iteration_count_++
          << " | T: " << std::fixed << std::setprecision(3) << temp;
    if (seam_move_prob >= 0.0) {
      *out_ << " | P(seam): " << std::setprecision(2) << seam_move_prob;
    }
    *out_ << " | Move: " << move_type << " | ";
  }

  void OnSeamMove(int next_shift) override {
    *out_ << "-> shift " << next_shift << " | ";
  }

  void OnAcceptance(bool accepted, double new_cost, double current_cost,
                    bool was_seam_move) override {
    if (was_seam_move) {
      // In unified SA, seam moves are always accepted to explore.
      *out_ << "Accepted (seam jump)";
    } else if (accepted) {
      if (new_cost < current_cost) {
        *out_ << "Accepted (better)";
      } else {
        *out_ << "Accepted (prob)";
      }
    } else {
      *out_ << "Rejected";
    }

    if (accepted) {
      *out_ << " | new_cost: " << std::fixed << std::setprecision(4)
            << new_cost;
    }

    if (accepted && new_cost < best_cost_) {
      best_cost_ = new_cost;
      *out_ << " (new best)";
    }
    *out_ << "\n";
  }

  void OnFinish(const SolutionStats& stats) override {
    *out_ << "--- Search Finished ---\n";
    if (stats.status == SolutionStats::NO_SOLUTION_FOUND) {
      *out_ << "Status: No solution found.\n";
    } else {
      *out_ << "Status: OK\n";
    }
    *out_ << "Paths evaluated: " << stats.paths_evaluated << "\n";
  }

 private:
  int iteration_count_ = 0;
  int invalid_count_ = 0;
  int current_shift_ = -1;
  bool current_is_reversed_ = false;
  double best_cost_ = std::numeric_limits<double>::infinity();
  double candidate_cost_ = std::numeric_limits<double>::infinity();
  int candidate_shift_ = -1;
  std::ostream* out_ = &std::cout;
};

class TriangulationVerboseVisitor : public RuledSurfaceVisitor {
 public:
  explicit TriangulationVerboseVisitor(std::ostream* out = &std::cout,
                                       bool dump_first_invalid = false)
      : out_(out), dump_first_invalid_(dump_first_invalid) {}

  VisitControl OnSeam(bool is_reversed) override {
    current_is_reversed_ = is_reversed;
    *out_ << "    Seam: " << (is_reversed ? "reversed" : "normal") << "\n";
    return kContinue;
  }

  VisitControl OnStart(const PolygonalChain&, const PolygonalChain&) override {
    *out_ << "--- Search Started ---\n";
    iteration_count_ = 0;
    invalid_count_ = 0;
    best_cost_ = std::numeric_limits<double>::infinity();
    return kContinue;
  }

  VisitControl OnPermutation(const Mesh& mesh) override {
    *out_ << "    Permutation visited.\n";
    return kContinue;
  }

  VisitControl OnInvalid(const Mesh& mesh) override {
    invalid_count_++;
    *out_ << "    Invalid solution #" << invalid_count_;
    if (mesh.is_empty()) {
      *out_ << " found (degenerate).\n";
    } else {
      *out_ << " found (self-intersecting).\n";
    }
    if (dump_first_invalid_ && invalid_count_ == 1) {
      *out_ << test::GetMeshAsObjString(mesh);
    }
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost) override {
    *out_ << "    Valid solution found with cost " << cost << ".\n";
    if (cost < best_cost_) {
      best_cost_ = cost;
      *out_ << "    New best solution found with cost " << cost << ".\n";
    }
    return kContinue;
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost, int shift) override {
    current_shift_ = shift;
    return OnValidMesh(mesh, cost);
  }

  VisitControl OnValidMesh(const Mesh& mesh, double cost,
                           const std::vector<bool>& moves) override {
    return OnValidMesh(mesh, cost);
  }

  void OnFinish(const SolutionStats& stats) override {
    *out_ << "--- Search Finished ---\n";
    if (stats.status == SolutionStats::NO_SOLUTION_FOUND) {
      *out_ << "Status: No solution found.\n";
    } else {
      *out_ << "Status: OK\n";
    }
    *out_ << "Paths evaluated: " << stats.paths_evaluated << "\n";
  }

 private:
  int iteration_count_ = 0;
  int invalid_count_ = 0;
  int current_shift_ = -1;
  bool current_is_reversed_ = false;
  double best_cost_ = std::numeric_limits<double>::infinity();
  std::ostream* out_ = &std::cout;
  bool dump_first_invalid_ = false;
};

}  // namespace ruled_surfaces
