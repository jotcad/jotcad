#pragma once

#include "ruled_surfaces_strategy_linear_helpers.h"

namespace ruled_surfaces {
namespace internal {
// SLG node (i,j) to index.
inline NodeIndex slg_node_to_index(int i, int j, int n) { return i * n + j; }
inline std::pair<int, int> slg_index_to_node(NodeIndex index, int n) {
  return {index / n, index % n};
}

<<<<<<< HEAD
=======

>>>>>>> main
PolygonSoup reconstruct_triangulation_slg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path);

std::vector<std::pair<PointCgal, PointCgal>> reconstruct_rulings_slg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path);

PolygonSoup reconstruct_triangulation_slg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path) {
  PolygonSoup soup;
  if (path.empty() || path.size() <= 1) return soup;

  int n_q = q.size();
  for (size_t k = 1; k < path.size(); ++k) {
    auto [i_prev, j_prev] = slg_index_to_node(path[k - 1].first, n_q);
    bool is_p_succeed = path[k].second;
    PointCgal p0 = p[i_prev];
    PointCgal p1 = q[j_prev];
    PointCgal p2;
    if (is_p_succeed) {
      auto segment = get_segment(p, i_prev);
      if (!segment) return {};
      p2 = segment->second;
    } else {
      auto segment = get_segment(q, j_prev);
      if (!segment) return {};
      p2 = segment->second;
    }

    soup.push_back({p0, p1, p2});
  }
  return soup;
}

std::vector<std::pair<PointCgal, PointCgal>> reconstruct_rulings_slg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path) {
  std::vector<std::pair<PointCgal, PointCgal>> rulings;
  if (path.empty()) return rulings;
  rulings.reserve(path.size());
  int n_q = q.size();
  for (const auto& step : path) {
    auto [i, j] = slg_index_to_node(step.first, n_q);
    if (i < p.size() && j < q.size()) {
      rulings.push_back({p[i], q[j]});
    }
  }
  return rulings;
}

template <typename Objective>
AdjacencyList build_graph_slg(const PolygonalChain& p, const PolygonalChain& q,
                              const Objective& objective) {
  int m = p.size();
  int n = q.size();
  AdjacencyList graph(m * n);
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      NodeIndex u = slg_node_to_index(i, j, n);
      // P-succeed to (i+1, j)
      if (i + 1 < m) {
        NodeIndex v = slg_node_to_index(i + 1, j, n);
        double cost = objective(p[i], q[j], p[i + 1]);
        graph[u].push_back({v, cost, true});
      }
      // Q-succeed to (i, j+1)
      if (j + 1 < n) {
        NodeIndex v = slg_node_to_index(i, j + 1, n);
        double cost = objective(p[i], q[j], q[j + 1]);
        graph[u].push_back({v, cost, false});
      }
    }
  }
  return graph;
}

// Yen's k-shortest path generator for SLG graphs.
template <typename Objective>
class YenPathGenerator {
 public:
  YenPathGenerator(const PolygonalChain& p, const PolygonalChain& q,
                   const Objective& objective,
                   std::string* decision_log = nullptr)
<<<<<<< HEAD
      : p_(p), q_(q), is_connected_(false), objective_(objective) {
=======
      : p_(p),
        q_(q),
        is_connected_(false),
        objective_(objective) {
>>>>>>> main
    graph_ = build_graph_slg(p_, q_, objective_);
    if (decision_log) {
      *decision_log += format_graph(graph_);
    }
    start_node_ = slg_node_to_index(0, 0, q_.size());
    end_node_ = slg_node_to_index(p_.size() - 1, q_.size() - 1, q_.size());
    is_connected_ = internal::is_connected(graph_, start_node_, end_node_);
  }

  bool is_graph_connected() const { return is_connected_; }

  // Returns k-th shortest path on each call, or invalid PathInfo if depleted.
  PathInfo next() {
    if (depleted_ || !is_connected_) return PathInfo{};

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
  void add_candidates_from_last_path() {
    PathInfo pk_1 = A_.back();
    for (size_t i = 0; i < pk_1.size() - 1; ++i) {
      NodeIndex spurNode = pk_1.steps[i].first;
      AdjacencyList mutable_graph = graph_;

      for (const auto& path_in_A : A_) {
        if (path_in_A.size() > i + 1 &&
            std::equal(pk_1.steps.begin(), pk_1.steps.begin() + i + 1,
                       path_in_A.steps.begin())) {
          NodeIndex u = path_in_A.steps[i].first;
          NodeIndex v = path_in_A.steps[i + 1].first;
          bool is_p = path_in_A.steps[i + 1].second;
          for (auto it = mutable_graph[u].begin();
               it != mutable_graph[u].end();) {
            if (it->to == v && it->is_p_succeed == is_p) {
              it = mutable_graph[u].erase(it);
            } else {
              ++it;
            }
          }
        }
      }
      PathInfo spurPath = dijkstra(mutable_graph, spurNode, end_node_);

      if (spurPath.is_valid() && spurPath.size() > 0) {
        std::vector<std::pair<NodeIndex, bool>> total_path_steps(
            pk_1.steps.begin(), pk_1.steps.begin() + i + 1);
        total_path_steps.insert(total_path_steps.end(),
                                spurPath.steps.begin() + 1,
                                spurPath.steps.end());
        double total_cost = 0;
        int n_q = q_.size();
        bool possible = true;
        for (size_t step_idx = 1; step_idx < total_path_steps.size();
             ++step_idx) {
          auto [i_prev, j_prev] =
              slg_index_to_node(total_path_steps[step_idx - 1].first, n_q);
          bool is_p = total_path_steps[step_idx].second;
          if (is_p) {
            if (i_prev + 1 >= p_.size()) {
              possible = false;
              break;
            }
            total_cost += objective_(p_[i_prev], q_[j_prev], p_[i_prev + 1]);
          } else {
            if (j_prev + 1 >= q_.size()) {
              possible = false;
              break;
            }
            total_cost += objective_(p_[i_prev], q_[j_prev], q_[j_prev + 1]);
          }
        }

        if (possible && B_set_.find(total_path_steps) == B_set_.end()) {
          B_.push({total_cost, total_path_steps});
          B_set_.insert(total_path_steps);
        }
      }
    }
  }

  const PolygonalChain& p_;
  const PolygonalChain& q_;
  AdjacencyList graph_;
  NodeIndex start_node_, end_node_;
  std::vector<PathInfo> A_;
  std::priority_queue<PathInfo, std::vector<PathInfo>, PathInfo::Greater> B_;
  std::set<std::vector<std::pair<NodeIndex, bool>>> B_set_;
  bool initialized_ = false;
  bool depleted_ = false;
  bool is_connected_ = false;
  Objective objective_;
};

<<<<<<< HEAD
}  // namespace internal
}  // namespace ruled_surfaces
=======
}
}
>>>>>>> main
