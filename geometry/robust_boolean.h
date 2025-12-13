#pragma once

#include <CGAL/IO/OBJ.h>  // For CGAL::IO::write_OBJ
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/intersection.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

#include <fstream>  // For std::ofstream
#include <iterator>
#include <map>
#include <set>
#include <vector>

#include "repair_util.h"
#include "self_touch_util.h"

template <typename K>
static bool robust_union(CGAL::Surface_mesh<typename K::Point_3>& target_mesh,
                         CGAL::Surface_mesh<typename K::Point_3>& source_mesh) {
  if (target_mesh.is_empty()) {
    if (source_mesh.is_empty()) {
      return true;  // Union of two empty meshes is empty
    }
    target_mesh = source_mesh;  // Union with empty target is source
    return true;
  }
  if (source_mesh.is_empty()) {
    return true;  // Union with empty source is target
  }
  assert(CGAL::Polygon_mesh_processing::does_bound_a_volume(target_mesh));
  assert(!PMP::does_self_intersect(target_mesh));
  assert(CGAL::Polygon_mesh_processing::does_bound_a_volume(source_mesh));
  assert(!PMP::does_self_intersect(source_mesh));
  const int MAX_ITERATIONS = 10;
  for (int iteration_count = 0; iteration_count < MAX_ITERATIONS;
       ++iteration_count) {
    std::vector<std::vector<typename K::Point_3>> polylines;
    PMP::surface_intersection(target_mesh, source_mesh,

                              std::back_inserter(polylines));
    std::vector<std::vector<typename K::Point_3>> touching_polylines;

    // Build a graph from the polylines to identify acyclic regions, which
    // correspond to touching regions.
    using Point = typename K::Point_3;
    std::map<Point, std::set<Point>> adj;
    std::set<Point> all_points;

    for (const auto& polyline : polylines) {
      if (polyline.size() == 1) {
        all_points.insert(polyline[0]);
      }
      if (polyline.size() < 2) {
        continue;
      }
      for (size_t i = 0; i < polyline.size() - 1; ++i) {
        const auto& p1 = polyline[i];
        const auto& p2 = polyline[i + 1];
        adj[p1].insert(p2);
        adj[p2].insert(p1);
        all_points.insert(p1);
        all_points.insert(p2);
      }
    }
    // Find connected components and check for acyclicity.
    std::set<Point> visited_points;
    std::vector<std::set<Point>> acyclic_components;
    int component_id = 0;
    for (const auto& start_point : all_points) {
      if (visited_points.count(start_point)) {
        continue;
      }
      component_id++;
      // Start traversal for a new component.
      std::set<Point> current_component_points;
      std::vector<Point> q;
      q.push_back(start_point);
      visited_points.insert(start_point);
      current_component_points.insert(start_point);
      size_t head = 0;
      long long edge_count = 0;
      while (head < q.size()) {
        Point curr = q[head++];
        if (adj.count(curr)) {
          edge_count += adj[curr].size();
          for (const auto& neighbor : adj[curr]) {
            if (visited_points.find(neighbor) == visited_points.end()) {
              visited_points.insert(neighbor);
              current_component_points.insert(neighbor);
              q.push_back(neighbor);
            }
          }
        }
      }
      size_t num_vertices = current_component_points.size();
      size_t num_edges = edge_count / 2;
      // In a connected component, V-1=E for a tree (acyclic).
      if (num_edges < num_vertices) {
        acyclic_components.push_back(current_component_points);
      }
    }
    // A polyline is a "touching" polyline if it belongs to an acyclic
    // component.
    for (const auto& polyline : polylines) {
      if (polyline.empty()) {
        continue;
      }
      bool is_touching = false;
      for (const auto& component : acyclic_components) {
        bool polyline_in_component = true;
        for (const auto& p : polyline) {
          if (component.find(p) == component.end()) {
            polyline_in_component = false;
            break;
          }
        }
        if (polyline_in_component) {
          is_touching = true;
          break;
        }
      }
      if (is_touching) {
        touching_polylines.push_back(polyline);
      }
    }
    if (touching_polylines.empty()) {
      break;
    }
    for (const auto& polyline : touching_polylines) {
      if (polyline.size() == 1) {
        const auto& p = polyline[0];
        CGAL::Surface_mesh<typename K::Point_3> single_point_envelope;
        make_point_envelope<K>(p, 0.01, single_point_envelope);

        if (!PMP::corefine_and_compute_difference(
                source_mesh, single_point_envelope, source_mesh,
                CGAL::parameters::all_default())) {
          std::cerr << "ERROR: robust_union: corefine_and_compute_difference "
                       "(point touch) failed."
                    << std::endl;
          return false;
        }
      } else if (polyline.size() > 1) {
        for (size_t i = 0; i < polyline.size() - 1; ++i) {
          const auto& p1 = polyline[i];
          const auto& p2 = polyline[i + 1];
          CGAL::Surface_mesh<typename K::Point_3> single_prism_mesh;
          make_segment_envelope<K>(p1, p2, 0.01, 0.01, single_prism_mesh);

          if (!PMP::corefine_and_compute_difference(
                  source_mesh, single_prism_mesh, source_mesh,
                  CGAL::parameters::all_default())) {
            std::cerr << "ERROR: robust_union: corefine_and_compute_difference "
                         "(edge touch) failed."
                      << std::endl;
            return false;
          }
        }
      }
    }
  }

  // Retry the original union operation with the now-modified source mesh.
  CGAL::Surface_mesh<typename K::Point_3> final_union_result;
  if (PMP::corefine_and_compute_union(target_mesh, source_mesh,
                                      final_union_result,
                                      CGAL::parameters::all_default())) {
    target_mesh = final_union_result;
    assert(!PMP::does_self_intersect(target_mesh));
    return true;
  }

  assert(false);
  return false;
}
