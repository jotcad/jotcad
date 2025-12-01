#pragma once

#include "ruled_surfaces_strategy_dlg_helpers.h"
#include "ruled_surfaces_strategy_linear_helpers.h"

namespace ruled_surfaces {
namespace internal {

// Yen's k-shortest path generator for DLG graphs.
class YenPathGeneratorDlg {
 public:
  YenPathGeneratorDlg(const PolygonalChain& p, const PolygonalChain& q,
                      const Objective& objective,
                      std::optional<bool> preceding_is_p_assumption)
      : p_(p),
        q_(q),
        objective_(objective),
        preceding_is_p_assumption_(preceding_is_p_assumption) {
    graph_ = internal::build_graph_dlg(p_, q_, objective_,
                                       preceding_is_p_assumption_);
    start_node_ = 2 * p_.size() * q_.size();
    end_node_ = 2 * p_.size() * q_.size() + 1;
  }

  YenPathGeneratorDlg(const PolygonalChain& p, const PolygonalChain& q,
                      const Objective& objective, AdjacencyList graph)
      : p_(p),
        q_(q),
        objective_(objective),
        preceding_is_p_assumption_(std::nullopt),
        graph_(std::move(graph)) {
    start_node_ = 2 * p_.size() * q_.size();
    end_node_ = 2 * p_.size() * q_.size() + 1;
  }

  // Returns k-th shortest path on each call, or invalid PathInfo if depleted.
  PathInfo next() {
    if (depleted_) return PathInfo{};

    if (!initialized_) {
      PathInfo shortest = dijkstra(graph_, start_node_, end_node_);
      if (!shortest.is_valid()) {
        depleted_ = true;
        return PathInfo{};
      }
      A_.push_back(shortest);
      initialized_ = true;
      add_candidates_from_last_path();
      return shortest;
    }

    if (B_.empty()) {
      depleted_ = true;
      return PathInfo{};
    }

    PathInfo result = B_.top();
    B_.pop();
    B_set_.erase(result.steps);
    A_.push_back(result);
    add_candidates_from_last_path();
    return result;
  }
  const AdjacencyList& get_graph() const { return graph_; }

 private:
  double get_edge_weight(NodeIndex u, NodeIndex v, bool is_p) {
    for (const auto& edge : graph_[u]) {
      if (edge.to == v && edge.is_p_succeed == is_p) return edge.weight;
    }
    return std::numeric_limits<double>::infinity();
  }

  void add_candidates_from_last_path() {
    PathInfo pk_1 = A_.back();
    for (size_t i = 0; i < pk_1.size() - 1; ++i) {
      NodeIndex spurNode = pk_1.steps[i].first;
      double rootPathCost = 0;
      for (size_t j = 1; j <= i; ++j) {
<<<<<<< HEAD
        rootPathCost += get_edge_weight(
            pk_1.steps[j - 1].first, pk_1.steps[j].first, pk_1.steps[j].second);
=======
        rootPathCost +=
            get_edge_weight(pk_1.steps[j - 1].first, pk_1.steps[j].first,
                            pk_1.steps[j].second);
>>>>>>> main
      }

      AdjacencyList mutable_graph = graph_;

      for (const auto& path_in_A : A_) {
        if (path_in_A.size() > i + 1 &&
            std::equal(pk_1.steps.begin(), pk_1.steps.begin() + i + 1,
                       path_in_A.steps.begin(),
                       [](const auto& a, const auto& b) {
                         return a.first == b.first;
                       })) {
          NodeIndex u_edge = path_in_A.steps[i].first;
          NodeIndex v_edge = path_in_A.steps[i + 1].first;
          bool is_p_edge = path_in_A.steps[i + 1].second;
          for (auto it = mutable_graph[u_edge].begin();
               it != mutable_graph[u_edge].end();) {
            if (it->to == v_edge && it->is_p_succeed == is_p_edge) {
              it = mutable_graph[u_edge].erase(it);
            } else {
              ++it;
            }
          }
        }
      }
      PathInfo spurPath = dijkstra(mutable_graph, spurNode, end_node_);

      if (spurPath.is_valid() && spurPath.size() > 0) {
        std::vector<std::pair<NodeIndex, bool>> total_path_steps;
        total_path_steps.reserve(i + 1 + spurPath.steps.size() - 1);
        for (size_t j = 0; j <= i; ++j)
          total_path_steps.push_back(pk_1.steps[j]);
        total_path_steps.insert(total_path_steps.end(),
                                spurPath.steps.begin() + 1,
                                spurPath.steps.end());

        double total_cost = rootPathCost + spurPath.cost;

        if (B_set_.find(total_path_steps) == B_set_.end()) {
          B_.push({total_cost, total_path_steps});
          B_set_.insert(total_path_steps);
        }
      }
    }
  }

  const PolygonalChain& p_;
  const PolygonalChain& q_;
  const Objective& objective_;
  std::optional<bool> preceding_is_p_assumption_;
  AdjacencyList graph_;
  NodeIndex start_node_, end_node_;
  std::vector<PathInfo> A_;
  std::priority_queue<PathInfo, std::vector<PathInfo>, PathInfo::Greater> B_;
  std::set<std::vector<std::pair<NodeIndex, bool>>> B_set_;
  bool initialized_ = false;
  bool depleted_ = false;
};
}  // namespace internal
<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces

>>>>>>> main
