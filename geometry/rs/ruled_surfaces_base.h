#pragma once

<<<<<<< HEAD
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

=======
>>>>>>> main
#include <algorithm>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <utility>
#include <variant>
#include <vector>

#include "objective.h"
#include "solution_stats.h"
#include "types.h"
<<<<<<< HEAD
=======
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_tree.h>
>>>>>>> main

namespace ruled_surfaces {

namespace internal {
inline double triangle_area(const PointCgal& p1, const PointCgal& p2,
                            const PointCgal& p3) {
  return 0.5 *
         std::sqrt(CGAL::cross_product(p2 - p1, p3 - p1).squared_length());
}

// Returns true if the three points of a triangle are collinear.
inline bool is_degenerate(const PointCgal& p1, const PointCgal& p2,
                          const PointCgal& p3) {
  return triangle_area(p1, p2, p3) < kEpsilon;
}

inline bool has_self_intersections(const Mesh& mesh) {
  return CGAL::Polygon_mesh_processing::does_self_intersect(mesh);
}

inline bool is_degenerate_mesh(const Mesh& mesh) {
  for (const auto& face : mesh.faces()) {
    std::vector<PointCgal> points;
    for (const auto& vertex : mesh.vertices_around_face(mesh.halfedge(face))) {
      points.push_back(mesh.point(vertex));
    }
    if (points.size() == 3 &&
        triangle_area(points[0], points[1], points[2]) < kEpsilon) {
      return true;
    }
  }
  return false;
}

inline bool is_manifold(const Mesh& mesh) {
  std::vector<typename Mesh::Halfedge_index> halfedges;
  CGAL::Polygon_mesh_processing::non_manifold_vertices(
      mesh, std::back_inserter(halfedges));
  return halfedges.empty();
}

inline Mesh soup_to_mesh(const PolygonSoup& soup) {
  Mesh mesh;
  std::vector<PointCgal> points;
  std::vector<std::vector<std::size_t>> faces_indices;
  std::map<PointCgal, std::size_t> point_to_idx;

  auto get_idx = [&](const PointCgal& p) {
    if (point_to_idx.find(p) == point_to_idx.end()) {
      point_to_idx[p] = points.size();
      points.push_back(p);
    }
    return point_to_idx[p];
  };

  for (const auto& face : soup) {
    faces_indices.push_back(
        {get_idx(face[0]), get_idx(face[1]), get_idx(face[2])});
  }

#ifndef NDEBUG
  for (const auto& face : faces_indices) {
    assert(face.size() >= 3 && "Invalid face size after point deduplication");
    assert(triangle_area(points[face[0]], points[face[1]], points[face[2]]) >=
           kEpsilon);
  }
#endif  // NDEBUG

  CGAL::Polygon_mesh_processing::repair_polygon_soup(points, faces_indices);
  CGAL::Polygon_mesh_processing::orient_polygon_soup(points, faces_indices);
  CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
      points, faces_indices, mesh);
  return mesh;
}

inline void soup_to_mesh(const PolygonSoup& soup, Mesh& mesh) {
  mesh.clear();
  std::vector<PointCgal> points;
  std::vector<std::vector<std::size_t>> faces_indices;
  std::map<PointCgal, std::size_t> point_to_idx;

  auto get_idx = [&](const PointCgal& p) {
    if (point_to_idx.find(p) == point_to_idx.end()) {
      point_to_idx[p] = points.size();
      points.push_back(p);
    }
    return point_to_idx[p];
  };

  for (const auto& face : soup) {
    faces_indices.push_back(
        {get_idx(face[0]), get_idx(face[1]), get_idx(face[2])});
  }

#ifndef NDEBUG
  for (const auto& face : faces_indices) {
    assert(face.size() >= 3 && "Invalid face size after point deduplication");
    assert(triangle_area(points[face[0]], points[face[1]], points[face[2]]) >=
           kEpsilon);
  }
#endif  // NDEBUG

  CGAL::Polygon_mesh_processing::repair_polygon_soup(points, faces_indices);
  CGAL::Polygon_mesh_processing::orient_polygon_soup(points, faces_indices);
  CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
      points, faces_indices, mesh);
}

// Stitches an open mesh (from two un-closed polygonal chains) into a closed
// mesh and calculates its cost. Returns std::nullopt if stitching fails.
template <typename Objective>
std::optional<std::pair<Mesh, double>> stitch_and_calculate_cost(
    const Mesh& open_mesh, const PolygonalChain& p_open,
    const PolygonalChain& q_open, const Objective& objective) {
  Mesh stitched_mesh = open_mesh;
  PointCgal p_start = p_open.front();
  PointCgal p_end = p_open.back();
  PointCgal q_start = q_open.front();
  PointCgal q_end = q_open.back();

  Mesh::Vertex_index v_p_start, v_p_end, v_q_start, v_q_end;
  bool p_start_found = false, p_end_found = false, q_start_found = false,
       q_end_found = false;

  for (Mesh::Vertex_index v : stitched_mesh.vertices()) {
    const auto& pt = stitched_mesh.point(v);
    if (pt == p_start) {
      v_p_start = v;
      p_start_found = true;
    }
    if (pt == p_end) {
      v_p_end = v;
      p_end_found = true;
    }
    if (pt == q_start) {
      v_q_start = v;
      q_start_found = true;
    }
    if (pt == q_end) {
      v_q_end = v;
      q_end_found = true;
    }
  }

  if (!p_start_found || !p_end_found || !q_start_found || !q_end_found) {
    return std::nullopt;
  }

  stitched_mesh.add_face(v_p_end, v_q_end, v_q_start);
  stitched_mesh.add_face(v_p_end, v_q_start, v_p_start);

  double closed_cost = objective.calculate_cost(stitched_mesh);
  return std::make_pair(stitched_mesh, closed_cost);
}
}  // namespace internal

<<<<<<< HEAD
}  // namespace ruled_surfaces
=======
} // namespace ruled_surfaces
>>>>>>> main
