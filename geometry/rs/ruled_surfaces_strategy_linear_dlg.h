#pragma once

#include "ruled_surfaces_strategy_dlg_helpers.h"
#include "ruled_surfaces_strategy_yen_dlg.h"
#include "visitor.h"

namespace ruled_surfaces {

template <typename Objective>
class LinearSearchDlg {
 public:
  using objective_type = Objective;

  struct Options {
    int max_total_paths;
  };

  LinearSearchDlg(const Objective& objective, const Options& options)
      : objective_(objective), options_(options) {}

  void generate(const PolygonalChain& p, const PolygonalChain& q,
                RuledSurfaceVisitor& visitor);

 private:
  Objective objective_;
  Options options_;
};

template <typename Objective>
void LinearSearchDlg<Objective>::generate(const PolygonalChain& p,
                                          const PolygonalChain& q,
                                          RuledSurfaceVisitor& visitor) {
  visitor.OnStart(p, q);
  struct GeneratorState {
    std::unique_ptr<internal::YenPathGeneratorDlg> gen;
    PolygonalChain p_chain;
    PolygonalChain q_chain;
    bool is_reversed;
    internal::PathInfo current_path;
    double prev_cost = -1.0;

    bool depleted() const { return !current_path.is_valid(); }
    double total_cost() const { return current_path.cost; }
    double gradient() const {
      if (prev_cost < 0.0 || !current_path.is_valid())
        return std::numeric_limits<double>::infinity();
      return current_path.cost - prev_cost;
    }
    void advance() {
      prev_cost = current_path.cost;
      current_path = gen->next();
    }
    struct Greater {
      bool operator()(const GeneratorState* a, const GeneratorState* b) const {
        return a->total_cost() > b->total_cost();
      }
    };
  };

  PolygonalChain q_rev = q;
  std::reverse(q_rev.begin(), q_rev.end());

  std::vector<std::unique_ptr<GeneratorState>> generators;
  for (bool is_rev : {false, true}) {
    PolygonalChain current_q = is_rev ? q_rev : q;
    auto state = std::make_unique<GeneratorState>();
    state->p_chain = p;
    state->q_chain = current_q;
    state->is_reversed = is_rev;
    state->gen = std::make_unique<internal::YenPathGeneratorDlg>(
        state->p_chain, state->q_chain, objective_, std::nullopt);
    state->current_path = state->gen->next();
    if (!state->depleted()) {
      generators.push_back(std::move(state));
    }
  }

  std::priority_queue<GeneratorState*, std::vector<GeneratorState*>,
                      typename GeneratorState::Greater>
      pq;
  for (auto& gen_ptr : generators) {
    pq.push(gen_ptr.get());
  }
  int paths_checked = 0;
  const double gradient_thresh = 1e-6;
  std::vector<GeneratorState*> sidelined_generators;

  while ((!pq.empty() || !sidelined_generators.empty()) &&
         paths_checked < options_.max_total_paths) {
    if (pq.empty()) {
      for (auto* gen : sidelined_generators) {
        if (!gen->depleted()) pq.push(gen);
      }
      sidelined_generators.clear();
      if (pq.empty()) {
        break;
      }
    }
    GeneratorState* top = pq.top();
    pq.pop();
    Mesh mesh = internal::soup_to_mesh(internal::reconstruct_triangulation_dlg(
        top->p_chain, top->q_chain, top->current_path.steps));

    paths_checked++;
    if (visitor.OnPermutation(mesh) == RuledSurfaceVisitor::kStop) break;

    if (!internal::has_self_intersections(mesh)) {
      if (visitor.OnSeam(top->is_reversed) == RuledSurfaceVisitor::kStop) break;
      if (visitor.OnValidMesh(mesh, top->total_cost()) ==
          RuledSurfaceVisitor::kStop) {
        break;
      }
    }

    bool flatten_out =
        (top->gradient() < gradient_thresh) && (top->total_cost() > 0);
    top->advance();

    if (flatten_out) {
      if (!top->depleted()) {
        sidelined_generators.push_back(top);
      }
    } else {
      for (auto* gen : sidelined_generators) {
        if (!gen->depleted()) pq.push(gen);
      }
      sidelined_generators.clear();
      if (!top->depleted()) {
        pq.push(top);
      }
    }
  }

  SolutionStats final_stats;
  final_stats.paths_evaluated = paths_checked;
  if (paths_checked >= options_.max_total_paths) {
    final_stats.status = SolutionStats::PATH_LIMIT_EXCEEDED;
  }
  visitor.OnFinish(final_stats);
}

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces
>>>>>>> main
