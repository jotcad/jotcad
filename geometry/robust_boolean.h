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

namespace PMP = CGAL::Polygon_mesh_processing;

// Helper function to save a mesh to an OBJ file for debugging

template <typename Mesh, typename Point>

void save_mesh_to_obj(const Mesh& mesh, const std::string& filename,
                      const std::vector<std::vector<Point>>& polylines) {
  std::ofstream outfile(filename);

  if (!outfile) {
    std::cerr << "ERROR: Could not open file for writing: " << filename
              << std::endl;

    return;
  }

  CGAL::IO::write_OBJ(outfile, mesh);  // Use CGAL::IO::write_OBJ

  // Add polylines as OBJ lines
  if (!polylines.empty()) {
    outfile << "# Polylines\n";
    int vertex_offset = mesh.number_of_vertices();  // Vertices from the mesh
                                                    // are 1-indexed in OBJ

    for (const auto& polyline : polylines) {
      if (polyline.empty()) continue;

      // Write polyline points as new vertices
      for (const auto& p : polyline) {
        outfile << "v " << p.x() << " " << p.y() << " " << p.z() << "\n";
      }

      // Write polyline as an OBJ line
      outfile << "l";  // Changed from 'L' to 'l'
      for (size_t i = 0; i < polyline.size(); ++i) {
        outfile << " "
                << (vertex_offset + 1 + i);  // OBJ vertices are 1-indexed
      }
      outfile << "\n";
      vertex_offset += polyline.size();
    }
  }

  outfile.close();

  std::cerr << "DEBUG: Saved mesh to " << filename << std::endl;
}

template <typename K>

static bool robust_union(CGAL::Surface_mesh<typename K::Point_3>& target_mesh,

                         CGAL::Surface_mesh<typename K::Point_3>& source_mesh) {
  std::cerr << "DEBUG: robust_union entered." << std::endl;

  if (target_mesh.is_empty()) {
    std::cerr << "DEBUG: robust_union: target_mesh is empty." << std::endl;

    if (source_mesh.is_empty()) {
      std::cerr
          << "DEBUG: robust_union: source_mesh is also empty. Returning true."
          << std::endl;

      return true;  // Union of two empty meshes is empty
    }

    std::cerr << "DEBUG: robust_union: Assigning source_mesh to target_mesh "
                 "and returning true."
              << std::endl;

    target_mesh = source_mesh;  // Union with empty target is source

    return true;
  }

  if (source_mesh.is_empty()) {
    std::cerr << "DEBUG: robust_union: source_mesh is empty. Returning true."
              << std::endl;

    return true;  // Union with empty source is target
  }

  // CGAL::Surface_mesh<typename K::Point_3> initial_union_result; // Not used
  // in current robust_union logic

  std::cerr << "DEBUG: robust_union: Assertions follow..." << std::endl;

  assert(CGAL::Polygon_mesh_processing::does_bound_a_volume(target_mesh));

  assert(!PMP::does_self_intersect(target_mesh));

  assert(CGAL::Polygon_mesh_processing::does_bound_a_volume(source_mesh));

  assert(!PMP::does_self_intersect(source_mesh));

  std::cerr
      << "DEBUG: robust_union: Assertions passed. Starting iterative trimming."
      << std::endl;

  const int MAX_ITERATIONS = 10;

  std::cerr << "DEBUG: robust_union: Entering trimming loop." << std::endl;

  for (int iteration_count = 0; iteration_count < MAX_ITERATIONS;
       ++iteration_count) {
    std::cerr << "DEBUG: robust_union: Iteration " << iteration_count
              << std::endl;

    std::vector<std::vector<typename K::Point_3>> polylines;

    std::cerr << "DEBUG: robust_union: Calling surface_intersection."
              << std::endl;

    PMP::surface_intersection(target_mesh, source_mesh,

                              std::back_inserter(polylines));

    std::cerr << "DEBUG: robust_union: surface_intersection returned "

              << polylines.size() << " polylines." << std::endl;

    for (size_t poly_idx = 0; poly_idx < polylines.size(); ++poly_idx) {
      const auto& polyline = polylines[poly_idx];

      bool is_closed_loop =

          polyline.size() >= 3 && polyline.front() == polyline.back();

      std::cerr << "DEBUG: Polyline " << poly_idx << ": "

                << "Points: " << polyline.size()

                << ", Closed Loop: " << (is_closed_loop ? "Yes" : "No")
                << std::endl;

      for (size_t pt_idx = 0; pt_idx < polyline.size(); ++pt_idx) {
        const auto& p = polyline[pt_idx];

        std::cerr << "  Point " << pt_idx << ": ("

                  << p.x() << ", " << p.y() << ", " << p.z() << ")"
                  << std::endl;
      }
    }

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

    std::cerr << "DEBUG: Analyzing polyline graph..." << std::endl;

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

      std::cerr << "DEBUG: Polyline graph component " << component_id << ": "
                << "Vertices=" << num_vertices << ", Edges=" << num_edges
                << ". ";

      // In a connected component, V-1=E for a tree (acyclic).
      if (num_edges < num_vertices) {
        std::cerr << "Classification: ACYCLIC (is a touch)." << std::endl;
        acyclic_components.push_back(current_component_points);
      } else {
        std::cerr << "Classification: CYCLIC (is an intersection)."
                  << std::endl;
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

    std::cerr << "DEBUG: robust_union: Found " << touching_polylines.size()
              << " touching polylines." << std::endl;

    if (touching_polylines.empty()) {
      std::cerr << "DEBUG: robust_union: No touching polylines. Breaking loop."
                << std::endl;

      break;
    }

    std::cerr << "DEBUG: robust_union: Trimming touching polylines."
              << std::endl;
    int point_touches_count = 0;
    int edge_touches_count = 0;
    for (const auto& polyline : touching_polylines) {
      if (polyline.size() == 1) {
        point_touches_count++;
        const auto& p = polyline[0];
        CGAL::Surface_mesh<typename K::Point_3> single_point_envelope;
        make_point_envelope<K>(p, 0.01, single_point_envelope);
        std::cerr << "DEBUG: robust_union: Trimming point touch." << std::endl;
        if (!PMP::corefine_and_compute_difference(
                source_mesh, single_point_envelope, source_mesh,
                CGAL::parameters::all_default())) {
          std::cerr << "ERROR: robust_union: corefine_and_compute_difference "
                       "(point touch) failed."
                    << std::endl;
          return false;
        }
      } else if (polyline.size() > 1) {
        edge_touches_count++;
        std::cerr << "DEBUG: robust_union: Trimming edge touch (polyline size: "
                  << polyline.size() << ")." << std::endl;
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
    std::cerr << "DEBUG: robust_union: Iteration " << iteration_count
              << " trimming complete. Point touches: " << point_touches_count
              << ", Edge touches: " << edge_touches_count << std::endl;

    // Save combined mesh for this iteration for review
    CGAL::Surface_mesh<typename K::Point_3> debug_combined_mesh_iter =
        target_mesh;
    debug_combined_mesh_iter.join(source_mesh);
    std::string filename = "debug_combined_for_review_iter_" +
                           std::to_string(iteration_count) + ".obj";
    save_mesh_to_obj(debug_combined_mesh_iter, filename, polylines);
    std::cerr << "DEBUG: robust_union: Created " << filename
              << " using Surface_mesh::join for iteration " << iteration_count
              << std::endl;
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
  // If we reach here, it means the initial union failed, and either there
  // were no touching polylines to repair, or the repair attempt also failed.
  std::cerr << "DEBUG: robust_union: Reached end of function, returning false."
            << std::endl;
  return false;
}
