#pragma once

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Line_3.h>
#include <CGAL/Plane_3.h>
#include <CGAL/Polygon_mesh_processing/autorefinement.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/merge_border_vertices.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <CGAL/Polygon_mesh_processing/repair_self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Segment_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Triangle_3.h>
#include <CGAL/intersections.h>
#include <CGAL/mark_domain_in_triangulation.h>

#include <array>
#include <iostream>  // Added for logging
#include <set>
#include <variant>

#include "self_touch_util.h"
#include "unit_util.h"

namespace PMP = CGAL::Polygon_mesh_processing;

template <typename K>
static void triangulate_planar_holes(
    CGAL::Surface_mesh<typename K::Point_3>& mesh,
    const std::vector<
        typename CGAL::Surface_mesh<typename K::Point_3>::Halfedge_index>&
        holes,
    const typename K::Plane_3& plane) {
  typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
  typedef typename Surface_mesh::Vertex_index Vertex_index;
  typedef CGAL::Constrained_Delaunay_triangulation_2<K, CGAL::Default,
                                                     CGAL::Exact_predicates_tag>
      CDT;

  CDT cdt;
  std::map<typename CDT::Vertex_handle, Vertex_index> vmap;

  for (auto h : holes) {
    auto curr = h;
    auto start_vh = cdt.insert(plane.to_2d(mesh.point(mesh.source(curr))));
    vmap[start_vh] = mesh.source(curr);
    auto prev_vh = start_vh;
    curr = mesh.next(curr);
    while (curr != h) {
      auto curr_vh = cdt.insert(plane.to_2d(mesh.point(mesh.source(curr))));
      vmap[curr_vh] = mesh.source(curr);
      cdt.insert_constraint(prev_vh, curr_vh);
      prev_vh = curr_vh;
      curr = mesh.next(curr);
    }
    cdt.insert_constraint(prev_vh, start_vh);
  }

  std::unordered_map<typename CDT::Face_handle, bool> in_domain_map;
  boost::associative_property_map<
      std::unordered_map<typename CDT::Face_handle, bool>>
      in_domain(in_domain_map);
  CGAL::mark_domain_in_triangulation(cdt, in_domain);

  for (auto f : cdt.finite_face_handles()) {
    if (get(in_domain, f)) {
      std::vector<Vertex_index> face_verts;
      face_verts.push_back(vmap[f->vertex(0)]);
      face_verts.push_back(vmap[f->vertex(1)]);
      face_verts.push_back(vmap[f->vertex(2)]);

      if (mesh.add_face(face_verts) == Surface_mesh::null_face()) {
        std::reverse(face_verts.begin(), face_verts.end());
        mesh.add_face(face_verts);
      }
    }
  }
}

template <typename K>
static bool number_of_holes(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  std::vector<typename CGAL::Surface_mesh<typename K::Point_3>::halfedge_index>
      border_cycles;
  CGAL::Polygon_mesh_processing::extract_boundary_cycles(
      mesh, std::back_inserter(border_cycles));
  return border_cycles.size();
}

template <typename K>
static size_t number_of_self_intersections(
    const CGAL::Surface_mesh<CGAL::Point_3<K>>& mesh) {
  std::vector<
      std::pair<typename CGAL::Surface_mesh<CGAL::Point_3<K>>::face_index,
                typename CGAL::Surface_mesh<CGAL::Point_3<K>>::face_index>>
      intersections;
  CGAL::Polygon_mesh_processing::self_intersections(
      mesh, std::back_inserter(intersections));
  return intersections.size();
}

template <typename Surface_mesh>
static int number_of_non_manifold_vertices(const Surface_mesh& mesh) {
  std::vector<typename Surface_mesh::Halfedge_index> vertices;
  CGAL::Polygon_mesh_processing::non_manifold_vertices(
      mesh, std::back_inserter(vertices));
  return vertices.size();
}

template <typename K>
static bool repair_self_intersection_by_autorefine(
    CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  CGAL::Surface_mesh<typename K::Point_3> working_mesh(mesh);
  // Should be precise.
  try {
    // Keep the autorefinements for later stages.
    std::cout << "repair_self_intersections: autorefine" << std::endl;
    if (CGAL::Polygon_mesh_processing::experimental::
            autorefine_and_remove_self_intersections(working_mesh)) {
      CGAL::Polygon_mesh_processing::remove_isolated_vertices(working_mesh);
      if (CGAL::is_valid_polygon_mesh(working_mesh)) {
        std::cout << "repair_self_intersections: autorefine succeeded"
                  << std::endl;
        mesh = std::move(working_mesh);
        return true;
      }
    }
  } catch (const std::exception& e) {
    std::cout << "repair_self_intersections: autorefine failed: " << e.what()
              << std::endl;
  }

  std::cout << "repair_self_intersections: count="
            << number_of_self_intersections(mesh) << std::endl;
  return false;
}

template <typename K>
static bool repair_degeneracies(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  return CGAL::Polygon_mesh_processing::remove_degenerate_edges(mesh) &&
         CGAL::Polygon_mesh_processing::remove_degenerate_faces(mesh);
}

template <typename K>
static bool repair_self_touches_xx(
    CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
  // This repairs cases where we have two corners that touch at one coordinate.

  // Duplicating the vertices fixes the topological problem, but not the
  // geometric problem. The duplicate vertices need to be moved so that they
  // have distinct coordinates. If we move the vertices we'll also move their
  // edges. Instead we split the edges near the vertex to be moved.
  std::cout << "Fixing non-manifold vertices" << std::endl;
  std::vector<std::vector<typename Surface_mesh::Vertex_index>> vertex_groups;
  CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(
      mesh,
      CGAL::parameters::output_iterator(std::back_inserter(vertex_groups)));
  std::cout << "  Debug: duplicate_non_manifold_vertices found "
            << vertex_groups.size() << " vertex groups." << std::endl;
  for (auto& vertex_group : vertex_groups) {
    for (auto& duplicated_vertex : vertex_group) {
      // Split each outgoing edge close to the duplicated vertex.
      std::vector<typename Surface_mesh::Halfedge_index> edges_to_split;
      // mesh.halfedge(vertex) produces an incoming edge, but we need an
      // outgoing edge.
      auto start = mesh.opposite(mesh.halfedge(duplicated_vertex));
      auto edge = start;

      const double kIota = 0.0001;
      double step_length = kIota * 2;

      // Walk around the vertex collecting edges.
      do {
        edges_to_split.push_back(edge);
        step_length =
            std::min(step_length,
                     CGAL::to_double(CGAL::Polygon_mesh_processing::edge_length(
                         edge, mesh)));
        edge = mesh.next_around_source(edge);
      } while (edge != start);

      // Bring it down to kIota or half edge length, whichever is smaller.
      step_length /= 2;

      // Split the edges, set positions, and accumulate average offset.
      typename K::Vector_3 sum(0, 0, 0);
      const typename K::Point_3 zero(0, 0, 0);
      for (const auto edge : edges_to_split) {
        auto source_point = mesh.point(mesh.source(edge));
        auto target_point = mesh.point(mesh.target(edge));
        // Split the edge.
        auto split = CGAL::Euler::split_edge(edge, mesh);
        auto vector = target_point - source_point;
        auto direction = unitVector(vector);
        auto offset = direction * step_length;
        // Place the split one step along the edge.
        auto position = source_point + offset;
        mesh.point(mesh.source(edge)) = position;
        // Accumulate the split offsets for averaging.
        sum += offset;
      }

      // Split faces between the edge split positions.
      for (size_t nth_edge = 0; nth_edge < edges_to_split.size(); nth_edge++) {
        auto edge = edges_to_split[nth_edge];
        if (mesh.face(mesh.opposite(edge)) == Surface_mesh::null_face()) {
          // This edge is on a border: we cannot split it.
          continue;
        }
        auto next = edges_to_split[(nth_edge + 1) % edges_to_split.size()];
        CGAL::Euler::split_face(mesh.opposite(edge), next, mesh);
      }

      // Move the vertex by half the average split offset.
      double size = edges_to_split.size();
      typename K::FT count(edges_to_split.size());
      typename K::Vector_3 average = sum / count;

      std::cout << "  Debug: kIota=" << kIota << ", step_length=" << step_length
                << std::endl;
      std::cout << "  Debug: Sum of offsets (" << sum.x() << ", " << sum.y()
                << ", " << sum.z() << "), Count=" << count << std::endl;
      std::cout << "  Debug: Average offset (" << average.x() << ", "
                << average.y() << ", " << average.z() << ")" << std::endl;
      std::cout << "  Debug: Moving duplicated_vertex ("
                << mesh.point(duplicated_vertex).x() << ", "
                << mesh.point(duplicated_vertex).y() << ", "
                << mesh.point(duplicated_vertex).z() << ") by average/2 ("
                << average.x() / 2 << ", " << average.y() / 2 << ", "
                << average.z() / 2 << ")" << std::endl;
      mesh.point(duplicated_vertex) += average / 2;
    }
  }
  CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
  return true;
}

template <typename K>
static bool repair_self_touching_edges(
    CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;

  std::cout << "Fixing non-manifold edges (via vertex repair)." << std::endl;

  std::vector<std::vector<typename Surface_mesh::Vertex_index>> vertex_groups;
  CGAL::Polygon_mesh_processing::duplicate_non_manifold_vertices(
      mesh,
      CGAL::parameters::output_iterator(std::back_inserter(vertex_groups)));
  if (vertex_groups.empty()) {
    // If no non-manifold vertices were found, then no repair was needed.
    return false;
  }
  for (auto& vertex_group : vertex_groups) {
    for (auto& duplicated_vertex : vertex_group) {
      // Split each outgoing edge close to the duplicated vertex.
      std::vector<typename Surface_mesh::Halfedge_index> edges_to_split;
      // mesh.halfedge(vertex) produces an incoming edge, but we need an
      // outgoing edge.
      auto start = mesh.opposite(mesh.halfedge(duplicated_vertex));
      auto edge = start;

      const double kIota = 0.0001;
      double step_length = kIota * 2;

      // Walk around the vertex collecting edges.
      do {
        edges_to_split.push_back(edge);
        step_length =
            std::min(step_length,
                     CGAL::to_double(CGAL::Polygon_mesh_processing::edge_length(
                         edge, mesh)));
        edge = mesh.next_around_source(edge);
      } while (edge != start);

      // Bring it down to kIota or half edge length, whichever is smaller.
      step_length /= 2;

      // Split the edges, set positions, and accumulate average offset.
      typename K::Vector_3 sum(0, 0, 0);
      const typename K::Point_3 zero(0, 0, 0);
      for (const auto edge_to_split : edges_to_split) {
        auto source_point = mesh.point(mesh.source(edge_to_split));
        auto target_point = mesh.point(mesh.target(edge_to_split));
        // Split the edge.
        auto split = CGAL::Euler::split_edge(edge_to_split, mesh);
        auto vector = target_point - source_point;
        auto direction = unitVector(vector);
        auto offset = direction * step_length;
        // Place the split one step along the edge.
        auto position = source_point + offset;
        mesh.point(mesh.source(edge_to_split)) = position;
        // Accumulate the split offsets for averaging.
        sum += offset;
      }

      // Split faces between the edge split positions.
      for (size_t nth_edge = 0; nth_edge < edges_to_split.size(); nth_edge++) {
        auto edge_to_split = edges_to_split[nth_edge];
        if (mesh.face(mesh.opposite(edge_to_split)) ==
            Surface_mesh::null_face()) {
          // This edge is on a border: we cannot split it.
          continue;
        }
        auto next = edges_to_split[(nth_edge + 1) % edges_to_split.size()];
        CGAL::Euler::split_face(mesh.opposite(edge_to_split), next, mesh);
      }

      // Move the vertex by half the average split offset.
      double size = edges_to_split.size();
      typename K::FT count(edges_to_split.size());
      typename K::Vector_3 average = sum / count;

      std::cout << "  Debug: kIota=" << kIota << ", step_length=" << step_length
                << std::endl;
      std::cout << "  Debug: Sum of offsets (" << sum.x() << ", " << sum.y()
                << ", " << sum.z() << "), Count=" << count << std::endl;
      std::cout << "  Debug: Average offset (" << average.x() << ", "
                << average.y() << ", " << average.z() << ")" << std::endl;
      std::cout << "  Debug: Moving duplicated_vertex ("
                << mesh.point(duplicated_vertex).x() << ", "
                << mesh.point(duplicated_vertex).y() << ", "
                << mesh.point(duplicated_vertex).z() << ") by average/2 ("
                << average.x() / 2 << ", " << average.y() / 2 << ", "
                << average.z() / 2 << ")" << std::endl;
      mesh.point(duplicated_vertex) += average / 2;
    }
  }
  CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
  return true;
}

template <typename K>
static void repair_zero_volume(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  CGAL::Polygon_mesh_processing::remove_connected_components_of_negligible_size(
      mesh, CGAL::parameters::volume_threshold(0.01).area_threshold(0));
}

template <typename K>
static bool repair_close_holes(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef CGAL::Surface_mesh<typename K::Point_3> Surface_mesh;
  typedef typename Surface_mesh::Halfedge_index Halfedge_index;
  typedef typename Surface_mesh::Vertex_index Vertex_index;
  typedef typename Surface_mesh::Face_index Face_index;
  typedef typename K::Point_3 Point_3;
  typedef typename K::Plane_3 Plane_3;
  typedef typename K::Vector_3 Vector_3;

  std::vector<Halfedge_index> border_cycles;
  CGAL::Polygon_mesh_processing::extract_boundary_cycles(
      mesh, std::back_inserter(border_cycles));

  if (border_cycles.empty()) return true;

  std::cout << "repair_close_holes: found " << border_cycles.size()
            << " cycles." << std::endl;

  // Group cycles by plane
  std::vector<std::vector<Halfedge_index>> groups;
  std::vector<Plane_3> planes;

  for (auto h : border_cycles) {
    // Estimate plane for this cycle
    std::vector<Point_3> points;
    auto curr = h;
    do {
      points.push_back(mesh.point(mesh.source(curr)));
      curr = mesh.next(curr);
    } while (curr != h);

    if (points.size() < 3) continue;

    Plane_3 plane;
    bool planar = false;
    // Simple planar check: use first 3 non-collinear points
    for (size_t i = 0; i < points.size() - 2; ++i) {
      if (!CGAL::collinear(points[i], points[i + 1], points[i + 2])) {
        plane = Plane_3(points[i], points[i + 1], points[i + 2]);
        planar = true;
        break;
      }
    }

    if (planar) {
      // Check if all points are on this plane
      for (const auto& p : points) {
        if (CGAL::squared_distance(plane, p) > 1e-9) {
          planar = false;
          break;
        }
      }
    }

    bool added = false;
    if (planar) {
      // Normalize plane normal
      Vector_3 n = plane.orthogonal_vector();
      double len = std::sqrt(CGAL::to_double(n.squared_length()));
      if (len > 0) n = n / len;
      // Re-construct normalized plane to get consistent 'd'
      plane = Plane_3(plane.point(), n);

      for (size_t i = 0; i < planes.size(); ++i) {
        // Check if planes are nearly identical
        // Normalize the existing plane normal for comparison
        Vector_3 ni = planes[i].orthogonal_vector();
        double leni = std::sqrt(CGAL::to_double(ni.squared_length()));
        if (leni > 0) ni = ni / leni;

        double dot = CGAL::to_double(ni * n);
        double dist_d = std::abs(CGAL::to_double(planes[i].d() - plane.d()));

        if (std::abs(dot) > 0.999 && dist_d < 1e-7) {
          groups[i].push_back(h);
          added = true;
          break;
        }
      }
    }

    if (!added) {
      groups.push_back({h});
      if (planar) {
        planes.push_back(plane);
      } else {
        // Placeholder plane for non-planar (won't be used for grouping)
        planes.push_back(Plane_3(0, 0, 1, 0));
      }
    }
  }

  std::cout << "repair_close_holes: " << groups.size() << " groups found."
            << std::endl;
  size_t remaining_holes = 0;
  for (size_t i = 0; i < groups.size(); ++i) {
    const auto& group = groups[i];
    std::cout << "Group " << i << ": " << group.size() << " cycles."
              << std::endl;
    if (group.size() == 1) {
      // Single hole: standard triangulation
      std::vector<Face_index> patch;
      CGAL::Polygon_mesh_processing::triangulate_hole(
          mesh, group[0],
          CGAL::parameters::face_output_iterator(std::back_inserter(patch)));
      if (patch.empty()) remaining_holes++;
    } else {
      std::cout << "triangulating planar group..." << std::endl;
      triangulate_planar_holes<K>(mesh, group, planes[i]);
    }
  }
  return remaining_holes == 0;
}

template <typename K>
static void repair_stitching(CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  CGAL::Polygon_mesh_processing::stitch_borders(mesh);
}
