#pragma once

#include "ruled_surfaces_strategy_linear_helpers.h"

namespace ruled_surfaces {
namespace internal {

// DLG node: layer 0 for P-path, 1 for Q-path
// Node idx = 2*(i*n+j)+layer corresponds to edge p_i q_j being active,
// having been reached by P-op (layer 0) or Q-op (layer 1).
inline int dlg_node_idx(int i, int j, int layer, int n) {
  return 2 * (i * n + j) + layer;
}
inline std::pair<int, int> dlg_get_ij(int node_idx, int n) {
  return {(node_idx / 2) / n, (node_idx / 2) % n};
}
inline int dlg_get_layer(int node_idx) { return node_idx % 2; }

AdjacencyList build_graph_dlg(const PolygonalChain& p, const PolygonalChain& q,
                              const Objective& objective,
                              std::optional<bool> preceding_is_p_assumption);

PolygonSoup reconstruct_triangulation_dlg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path) {
  PolygonSoup soup;
  if (path.empty() || path.size() <= 2) return soup;  // S -> ... -> E

  int n_q = q.size();
  int virtual_start_node = 2 * p.size() * n_q;

  // path[0] is S, path[1...size-2] are DLG nodes, path[size-1] is E
  for (size_t k = 1; k < path.size() - 1; ++k) {
    bool is_p_op = path[k].second;
    int i_prev, j_prev;
    if (path[k - 1].first == virtual_start_node) {
      i_prev = 0;
      j_prev = 0;
    } else {
      auto ij = internal::dlg_get_ij(path[k - 1].first, n_q);
      i_prev = ij.first;
      j_prev = ij.second;
    }
    if (is_p_op) {
      PointCgal p_next = get_segment(p, i_prev)->second;
      assert(p[i_prev] != q[j_prev] && p[i_prev] != p_next &&
             q[j_prev] != p_next);
      assert(!internal::is_degenerate(p[i_prev], q[j_prev], p_next));
      soup.push_back({p[i_prev], q[j_prev], p_next});
    } else {
      PointCgal q_next = get_segment(q, j_prev)->second;
      assert(p[i_prev] != q[j_prev] && p[i_prev] != q_next &&
             q[j_prev] != q_next);
      assert(!internal::is_degenerate(p[i_prev], q[j_prev], q_next));
      soup.push_back({p[i_prev], q[j_prev], q_next});
    }
  }
  return soup;
}

inline std::vector<std::pair<PointCgal, PointCgal>> reconstruct_rulings_dlg(
    const PolygonalChain& p, const PolygonalChain& q,
    const std::vector<std::pair<NodeIndex, bool>>& path) {
  std::vector<std::pair<PointCgal, PointCgal>> rulings;
  if (p.empty() || q.empty() || path.empty() || path.size() <= 2) {
    return rulings;
  }
  rulings.reserve(path.size() - 1);
  rulings.push_back({p[0], q[0]});

  int n_q = q.size();
  for (size_t k = 1; k < path.size() - 1; ++k) {
    auto ij = internal::dlg_get_ij(path[k].first, n_q);
    int i = ij.first;
    int j = ij.second;
    if (i < p.size() && j < q.size()) {
      rulings.push_back({p[i], q[j]});
    }
  }
  return rulings;
}

AdjacencyList build_graph_dlg(const PolygonalChain& p, const PolygonalChain& q,
                              const Objective& objective,
                              std::optional<bool> preceding_is_p_assumption) {
  int m = p.size();
  int n = q.size();
  AdjacencyList graph(2 * m * n + 2);
  const NodeIndex S = 2 * m * n;
  const NodeIndex E = 2 * m * n + 1;

  double p_cost_at_start = 0.0;
  double q_cost_at_start = 0.0;
  if (preceding_is_p_assumption.has_value()) {
    bool last_step_is_p = preceding_is_p_assumption.value();
    if (m > 1) {
      p_cost_at_start = objective.get_dihedral_cost(
          p[0], q[0], last_step_is_p ? p[m - 2] : q[n - 2], p[1]);
    }
    if (n > 1) {
      q_cost_at_start = objective.get_dihedral_cost(
          p[0], q[0], last_step_is_p ? p[m - 2] : q[n - 2], q[1]);
    }
  }

  if (m > 1)
    graph[S].push_back({dlg_node_idx(1, 0, 0, n), p_cost_at_start, true});
  if (n > 1)
    graph[S].push_back({dlg_node_idx(0, 1, 1, n), q_cost_at_start, false});

  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      NodeIndex u0 = dlg_node_idx(i, j, 0, n);  // Arrived via P-succeed
      NodeIndex u1 = dlg_node_idx(i, j, 1, n);  // Arrived via Q-succeed

      if (i + 1 < m) {
        NodeIndex v = dlg_node_idx(i + 1, j, 0, n);
        double cost0 = (i > 0) ? objective.get_dihedral_cost(p[i], q[j],
                                                             p[i - 1], p[i + 1])
                               : 0;
        double cost1 = (j > 0) ? objective.get_dihedral_cost(p[i], q[j],
                                                             q[j - 1], p[i + 1])
                               : 0;
        graph[u0].push_back({v, cost0, true});
        graph[u1].push_back({v, cost1, true});
      }
      if (j + 1 < n) {
        NodeIndex v = dlg_node_idx(i, j + 1, 1, n);
        double cost0 = (i > 0) ? objective.get_dihedral_cost(p[i], q[j],
                                                             p[i - 1], q[j + 1])
                               : 0;
        double cost1 = (j > 0) ? objective.get_dihedral_cost(p[i], q[j],
                                                             q[j - 1], q[j + 1])
                               : 0;
        graph[u0].push_back({v, cost0, false});
        graph[u1].push_back({v, cost1, false});
      }
    }
  }

  NodeIndex end_p_layer = dlg_node_idx(m - 1, n - 1, 0, n);
  NodeIndex end_q_layer = dlg_node_idx(m - 1, n - 1, 1, n);
  graph[end_p_layer].push_back({E, 0.0, true});
  graph[end_q_layer].push_back({E, 0.0, false});
  return graph;
}

}  // namespace internal
}  // namespace ruled_surfaces
