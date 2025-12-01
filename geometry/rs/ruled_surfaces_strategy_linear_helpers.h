#pragma once

#include "ruled_surfaces_base.h"

namespace ruled_surfaces {
namespace internal {

// Graph representation for Dijkstra
using NodeIndex = int;
using Weight = double;
struct Edge {
  NodeIndex to;
  Weight weight;
  bool is_p_succeed;  // true if P-succeed, false if Q-succeed
};
using AdjacencyList = std::vector<std::vector<Edge>>;

inline std::string format_graph(const AdjacencyList& graph) {
  std::stringstream ss;
  ss << "Graph Adjacency List:\n";
  for (NodeIndex i = 0; i < graph.size(); ++i) {
    if (graph[i].empty()) continue;
    ss << i << " -> {";
    for (size_t j = 0; j < graph[i].size(); ++j) {
      ss << graph[i][j].to;
      if (j < graph[i].size() - 1) ss << ", ";
    }
    ss << "}\n";
  }
  return ss.str();
}

// Represents a path found by Dijkstra or Yen's
struct PathInfo {
  double cost = std::numeric_limits<double>::infinity();
  std::vector<std::pair<NodeIndex, bool>> steps;
  bool is_valid() const {
    return cost != std::numeric_limits<double>::infinity() && !steps.empty();
  }
  bool empty() const { return steps.empty(); }
  size_t size() const { return steps.size(); }

  // Min-heap comparator for priority queue
  struct Greater {
    bool operator()(const PathInfo& a, const PathInfo& b) const {
      return a.cost > b.cost;
    }
  };
};

inline PolygonalChain rotate_chain(const PolygonalChain& chain, int shift) {
  if (chain.size() <= 2 || shift == 0) return chain;
  PolygonalChain rotated_chain;
  rotated_chain.reserve(chain.size());
  for (size_t i = 0; i < chain.size() - 1; ++i) {
    rotated_chain.push_back(chain[(i + shift) % (chain.size() - 1)]);
  }
  rotated_chain.push_back(rotated_chain.front());
  return rotated_chain;
}

inline PolygonalChain reverse_chain(const PolygonalChain& chain) {
  if (chain.size() <= 2) return chain;
  bool is_closed = chain.size() > 2 && chain.front() == chain.back();
  if (!is_closed) {
    PolygonalChain reversed_chain = chain;
    std::reverse(reversed_chain.begin(), reversed_chain.end());
    return reversed_chain;
  }

  PolygonalChain reversed_chain;
  reversed_chain.reserve(chain.size());
  reversed_chain.push_back(chain[0]);
  for (int i = chain.size() - 2; i >= 1; --i) {
    reversed_chain.push_back(chain[i]);
  }
  reversed_chain.push_back(reversed_chain.front());
  return reversed_chain;
}

// Get nth segment of a polyline.
// If chain is closed, n is taken modulo number of segments.
// If chain is open, n must be in [0, chain.size()-2].
// Returns nullopt if n is out of range for open chain, or if chain has < 2
// points.
inline std::optional<std::pair<PointCgal, PointCgal>> get_segment(
    const PolygonalChain& chain, int n) {
  if (chain.size() < 2) {
    return std::nullopt;
  }
  bool is_closed = chain.size() > 2 && chain.front() == chain.back();
  int num_segments = chain.size() - 1;
  int segment_idx = n;
  if (is_closed) {
    segment_idx = (n % num_segments + num_segments) % num_segments;
  } else if (n < 0 || n >= num_segments) {
    return std::nullopt;
  }
  return std::make_pair(chain[segment_idx], chain[segment_idx + 1]);
}

inline bool is_connected(const AdjacencyList& graph, NodeIndex start_node,
                         NodeIndex end_node) {
  if (start_node >= graph.size() || end_node >= graph.size()) return false;
  std::vector<bool> visited(graph.size(), false);
  std::queue<NodeIndex> q;
  q.push(start_node);
  visited[start_node] = true;
  while (!q.empty()) {
    NodeIndex u = q.front();
    q.pop();
    if (u == end_node) return true;
    for (const auto& edge : graph[u]) {
      if (!visited[edge.to]) {
        visited[edge.to] = true;
        q.push(edge.to);
      }
    }
  }
  return false;
}

// Dijkstra's algorithm
inline PathInfo dijkstra(const AdjacencyList& graph, NodeIndex start_node,
                         NodeIndex end_node) {
  if (start_node >= graph.size() || end_node >= graph.size()) {
    return PathInfo{};
  }
  std::vector<Weight> dist(graph.size(),
                           std::numeric_limits<Weight>::infinity());
  std::vector<NodeIndex> pred(graph.size(), -1);
  std::vector<bool> pred_is_p_succeed(graph.size(), false);
  dist[start_node] = 0;

  using State = std::pair<Weight, NodeIndex>;
  std::priority_queue<State, std::vector<State>, std::greater<State>> pq;
  pq.push({0, start_node});

  while (!pq.empty()) {
    auto [d, u] = pq.top();
    pq.pop();

    if (d > dist[u]) continue;
    if (u == end_node) break;

    for (const auto& edge : graph[u]) {
      if (dist[u] + edge.weight < dist[edge.to]) {
        dist[edge.to] = dist[u] + edge.weight;
        pred[edge.to] = u;
        pred_is_p_succeed[edge.to] = edge.is_p_succeed;
        pq.push({dist[edge.to], edge.to});
      }
    }
  }

  // Reconstruct path
  std::vector<std::pair<NodeIndex, bool>> path_steps;
  if (dist[end_node] == std::numeric_limits<Weight>::infinity()) {
    return PathInfo{};  // No path found
  }
  NodeIndex current = end_node;
  while (current != start_node) {
    path_steps.push_back({current, pred_is_p_succeed[current]});
    current = pred[current];
  }
  path_steps.push_back({start_node, false});
  std::reverse(path_steps.begin(), path_steps.end());
  return PathInfo{dist[end_node], path_steps};
}

}  // namespace internal
}  // namespace ruled_surfaces
