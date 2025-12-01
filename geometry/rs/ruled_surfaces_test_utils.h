#pragma once

#include <CGAL/Aff_transformation_3.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Surface_mesh.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "ruled_surfaces_base.h"
#include "ruled_surfaces_objective_min_area.h"
#include "ruled_surfaces_sa_stopping_rules.h"
#include "ruled_surfaces_strategy_seam_search_all.h"
#include "ruled_surfaces_strategy_seam_search_sa.h"
#include "ruled_surfaces_strategy_slg_helpers.h"

namespace ruled_surfaces {

/**
 * @brief An objective transformer that modifies the cost returned by a
 * wrapped objective using the formula: cost = base + value * scale.
 *
 * This is useful for adapting objectives to fit algorithms with specific
 * requirements, such as converting a maximization objective into a
 * minimization objective for use with shortest-path algorithms like
 * LinearSearchSlg.
 *
 * Examples:
 * - To use a minimization objective 'MinObj' as is:
 *   ObjectiveTx<MinObj, 0.0, 1.0> tx; -> cost = 0.0 + value * 1.0 = value
 * - To convert a maximization objective 'MaxObj' (score in [0, 1])
 *   to a minimization objective with non-negative costs for Dijkstra:
 *   ObjectiveTx<MaxObj, 1.0, -1.0> tx; -> cost = 1.0 + value * (-1.0) = 1.0 -
 * value
 *
 * @tparam Objective The objective type to wrap (e.g., MinArea, MaxConvexity).
 * @tparam base The additive constant in the transformation.
 * @tparam scale The multiplicative constant in the transformation.
 */
namespace test {

inline bool IsManifold(const Mesh& mesh) {
  std::vector<Mesh::Halfedge_index> non_manifold_halfedges;
  CGAL::Polygon_mesh_processing::non_manifold_vertices(
      mesh, std::back_inserter(non_manifold_halfedges));
  return non_manifold_halfedges.empty();
}

// Returns true if mesh has self-intersections among non-adjacent faces.
inline bool IsSelfIntersecting(const Mesh& mesh) {
  return CGAL::Polygon_mesh_processing::does_self_intersect(mesh);
}

struct ObjExportOptions {
  bool include_polylines = true;
};

inline std::string GetMeshAsObjString(
    const Mesh& mesh, const std::vector<PolygonalChain>& polylines = {},
    const ObjExportOptions& options = {}) {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(6);
  std::map<PointCgal, int> point_to_idx;
  int next_vertex_idx = 1;

  std::vector<std::pair<PointCgal, PointCgal>> local_rulings;
  if (polylines.size() == 2 && !polylines[0].empty() && !polylines[1].empty()) {
    local_rulings.push_back({polylines[0].front(), polylines[1].front()});
  }

  auto process_point = [&](const PointCgal& p) {
    if (point_to_idx.find(p) == point_to_idx.end()) {
      ss << "v " << p.x() << " " << p.y() << " " << p.z() << "\n";
      point_to_idx[p] = next_vertex_idx++;
    }
  };

  for (auto v : mesh.vertices()) {
    process_point(mesh.point(v));
  }
  for (const auto& chain : polylines) {
    for (const auto& pt : chain) {
      process_point(pt);
    }
  }
  for (const auto& ruling : local_rulings) {
    process_point(ruling.first);
    process_point(ruling.second);
  }

  // Write mesh faces
  for (auto f : mesh.faces()) {
    ss << "f";
    for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
      ss << " " << point_to_idx.at(mesh.point(v));
    }
    ss << "\n";
  }
  for (const auto& ruling : local_rulings) {
    auto it1 = point_to_idx.find(ruling.first);
    auto it2 = point_to_idx.find(ruling.second);
    if (it1 != point_to_idx.end() && it2 != point_to_idx.end()) {
      ss << "l " << it1->second << " " << it2->second << "\n";
    }
  }

  // If requested, emit polylines as OBJ line segments ('l').
  // For debugging purposes, closed polylines are also emitted as faces ('f').
  if (options.include_polylines) {
    for (const auto& chain : polylines) {
      if (chain.empty()) continue;
      ss << "l";
      for (const auto& pt : chain) {
        ss << " " << point_to_idx.at(pt);
      }
      ss << "\n";
      if (chain.size() >= 4 && chain.front() == chain.back()) {
        ss << "f";
        for (size_t i = 0; i < chain.size() - 1; ++i) {
          ss << " " << point_to_idx.at(chain[i]);
        }
        ss << "\n";
      }
    }
  }

  if (ss.str().empty()) return "\n\n";
  return "\n" + ss.str();
}

inline std::pair<PolygonalChain, PolygonalChain> CreateUnitSquareGeometry() {
  PolygonalChain p = {PointCgal(0, 0, 0), PointCgal(1, 0, 0)};
  PolygonalChain q = {PointCgal(0, 1, 0), PointCgal(1, 1, 0)};
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain> CreateUnitCubeSidesGeometry() {
  PolygonalChain p = {PointCgal(0, 0, 0), PointCgal(1, 0, 0),
                      PointCgal(1, 1, 0), PointCgal(0, 1, 0),
                      PointCgal(0, 0, 0)};
  PolygonalChain q = {PointCgal(0, 0, 1), PointCgal(1, 0, 1),
                      PointCgal(1, 1, 1), PointCgal(0, 1, 1),
                      PointCgal(0, 0, 1)};
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain> CreateRotatedCrescentsGeometry(
    int num_segments) {
  const int num_points = num_segments + 1;
  const double radius = 1.0;
  const double pi = std::acos(-1.0);
  const double angle_start = -2.0 * pi / 3.0;
  const double angle_end = 2.0 * pi / 3.0;
  const double angle_step = (angle_end - angle_start) / num_segments;

  PolygonalChain p, q;
  for (int i = 0; i < num_points; ++i) {
    double angle = angle_start + i * angle_step;
    double ca = std::cos(angle);
    double sa = std::sin(angle);
    p.push_back(PointCgal(radius * ca, radius * sa, 0.0));
    q.push_back(PointCgal(-radius * ca, -radius * sa, 1.0));
  }
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain>
CreateRotatedClosedCrescentsGeometry(int num_arc_segments) {
  const double pi = std::acos(-1.0);
  const double outer_radius = 1.0;
  const double angle_start = -2.0 * pi / 3.0;
  const double angle_end = 2.0 * pi / 3.0;
  const double outer_angle_step = (angle_end - angle_start) / num_arc_segments;
  const double inner_radius = std::sqrt(0.8125);
  const double inner_cx = -0.75;
  const double inner_angle = atan2(outer_radius * sin(angle_end),
                                   outer_radius * cos(angle_end) - inner_cx);
  const double inner_angle_step = (2 * inner_angle) / num_arc_segments;

  PolygonalChain p_pts, q_pts;
  for (int i = 0; i <= num_arc_segments; ++i) {
    double angle = angle_start + i * outer_angle_step;
    double ca = std::cos(angle);
    double sa = std::sin(angle);
    p_pts.push_back(PointCgal(outer_radius * ca, outer_radius * sa, 0.0));
    q_pts.push_back(PointCgal(-outer_radius * ca, -outer_radius * sa, 1.0));
  }
  for (int i = 1; i < num_arc_segments; ++i) {
    double angle = inner_angle - i * inner_angle_step;
    double ca = std::cos(angle);
    double sa = std::sin(angle);
    p_pts.push_back(
        PointCgal(inner_cx + inner_radius * ca, inner_radius * sa, 0.0));
    q_pts.push_back(
        PointCgal(-(inner_cx + inner_radius * ca), -inner_radius * sa, 1.0));
  }
  PolygonalChain p = p_pts;
  p.push_back(p.front());
  PolygonalChain q = q_pts;
  q.push_back(q.front());
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain>
CreateRotatedOpenCrescentsGeometry(int num_arc_segments) {
  auto [p_closed, q_closed] =
      CreateRotatedClosedCrescentsGeometry(num_arc_segments);
  PolygonalChain p_open(p_closed.begin(), p_closed.end() - 1);
  PolygonalChain q_open(q_closed.begin(), q_closed.end() - 1);
  return {p_open, q_open};
}

inline std::pair<PolygonalChain, PolygonalChain>
CreateIdeallyRotatedClosedCrescents3Geometry() {
  auto [p, q] = CreateRotatedClosedCrescentsGeometry(3);
  return {internal::rotate_chain(p, 3), q};
}

// Creates two congruent, closed star-shaped polygons. One polygon is rotated
// relative to the other and displaced along the z-axis. This configuration is
// useful for testing ruled surface algorithms because the resulting surface
// can have complex self-intersections and regions of both positive and
// negative curvature, challenging the algorithm's robustness.
inline std::pair<PolygonalChain, PolygonalChain> CreateTwistedStarsGeometry(
    int n_points, int n_lobes, double amplitude) {
  PolygonalChain p, q;
  const double pi = std::acos(-1.0);
  double step = 2 * pi / n_points;
  for (int i = 0; i < n_points; ++i) {
    double angle = i * step;
    double r = 1.0 + amplitude * cos(n_lobes * angle);
    p.push_back(PointCgal(r * cos(angle), r * sin(angle), 0.0));
    double angle_q = angle + pi / n_lobes;
    q.push_back(PointCgal(r * cos(angle_q), r * sin(angle_q), 1.0));
  }
  p.push_back(p.front());
  q.push_back(q.front());
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain> CreateWavyRingsGeometry(
    int n_points, double amplitude, double frequency) {
  PolygonalChain p, q;
  const double radius = 1.5;
  const double pi = std::acos(-1.0);
  double step = 2 * pi / n_points;
  for (int i = 0; i < n_points; ++i) {
    double angle = i * step;
    p.push_back(PointCgal(radius * cos(angle), radius * sin(angle), 0));
    q.push_back(PointCgal(radius * cos(angle), radius * sin(angle),
                          1.0 + amplitude * sin(frequency * angle)));
  }
  p.push_back(p.front());
  q.push_back(q.front());
  return {p, q};
}

inline PolygonalChain generate_irregular_loop(
    int num_points, double r0, const std::vector<double>& r_a,
    const std::vector<double>& r_f, const std::vector<double>& r_p, double z0,
    const std::vector<double>& z_a, const std::vector<double>& z_f,
    const std::vector<double>& z_p) {
  PolygonalChain chain;
  chain.reserve(num_points);
  const double pi = std::acos(-1.0);
  double angle_step = 2.0 * pi / (num_points - 1);
  for (int i = 0; i < num_points - 1; ++i) {
    double angle = i * angle_step;
    double r = r0;
    for (int j = 0; j < r_a.size(); ++j) {
      r += r_a[j] * std::sin(r_f[j] * angle + r_p[j]);
    }
    double z = z0;
    for (int j = 0; j < z_a.size(); ++j) {
      z += z_a[j] * std::sin(z_f[j] * angle + z_p[j]);
    }
    chain.push_back(PointCgal(r * std::cos(angle), r * std::sin(angle), z));
  }
  chain.push_back(chain[0]);
  return chain;
}

inline std::pair<PolygonalChain, PolygonalChain>
generate_complex_asymmetric_loops(int p_points, int q_points,
                                  double radius = 1.0, double z_sep = 0.5) {
  const double pi = std::acos(-1.0);
  PolygonalChain p = generate_irregular_loop(
      p_points, radius, {radius * 0.1, radius * 0.05}, {3.0, 5.0},
      {0.0, pi / 2.0}, 0.0, {radius * 0.1}, {2.0}, {0.0});
  PolygonalChain q = generate_irregular_loop(
      q_points, radius * 1.1, {radius * 0.08, radius * 0.06}, {4.0, 6.0},
      {0.0, pi / 2.0}, z_sep, {radius * 0.12}, {3.0}, {0.0});
  return {p, q};
}

inline std::pair<PolygonalChain, PolygonalChain>
CreateHighlyIrregularLoopsGeometry() {
  PolygonalChain p = generate_irregular_loop(
      50, 1.0, {0.2, 0.1}, {7.0, 13.0}, {0.0, 0.5}, 0.0, {0.1}, {5.0}, {0.0});
  PolygonalChain q = generate_irregular_loop(
      60, 1.0, {0.25, 0.15}, {5.0, 11.0}, {0.2, 0.7}, 0.8, {0.1}, {7.0}, {0.0});
  return {p, q};
}

inline std::pair<std::vector<PolygonalChain>, std::vector<PolygonalChain>>
generate_multiple_loops(int num_loops, int num_points_per_loop,
                        double loop_radius, unsigned int seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<> coord_dist(-0.05, 0.05);
  std::vector<PolygonalChain> p_chains, q_chains;
  for (int i = 0; i < num_loops; ++i) {
    double offset_x = i * loop_radius * 3;
    PolygonalChain p_loop, q_loop;
    p_loop.reserve(num_points_per_loop);
    q_loop.reserve(num_points_per_loop);

    const double pi = std::acos(-1.0);
    double angle_step = 2.0 * pi / (num_points_per_loop - 1);
    for (int j = 0; j < num_points_per_loop - 1; ++j) {
      double angle = j * angle_step;
      p_loop.push_back(
          PointCgal(offset_x + loop_radius * std::cos(angle) + coord_dist(rng),
                    loop_radius * std::sin(angle) + coord_dist(rng),
                    0.0 + coord_dist(rng)));
      q_loop.push_back(
          PointCgal(offset_x + loop_radius * std::cos(angle) + coord_dist(rng),
                    loop_radius * std::sin(angle) + coord_dist(rng),
                    1.0 + coord_dist(rng)));
    }
    p_loop.push_back(p_loop.front());
    q_loop.push_back(q_loop.front());
    p_chains.push_back(p_loop);
    q_chains.push_back(q_loop);
  }

  std::shuffle(q_chains.begin(), q_chains.end(), rng);

  return {p_chains, q_chains};
}

}  // namespace test
}  // namespace ruled_surfaces
