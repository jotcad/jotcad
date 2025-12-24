#pragma once

#include <CGAL/AABB_traits_3.h>              // Required for AABB_traits_3
#include <CGAL/AABB_tree.h>                  // Required for AABB_tree
#include <CGAL/AABB_triangle_primitive_3.h>  // Required for AABB_triangle_primitive_3
#include <CGAL/Cartesian_converter.h>        // Added for to_epeck converter
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/IO/OBJ.h>  // Added for write_soup_to_obj
#include <CGAL/Polygon_mesh_processing/autorefinement.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>  // Required for self_intersections
#include <CGAL/Ray_3.h>  // Needed for Ray_3

#include <array>
#include <cmath>    // For std::sqrt
#include <fstream>  // Added for std::ofstream
#include <iomanip>  // Added for std::fixed and std::setprecision
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>  // Added for std::runtime_error
#include <string>     // Added for std::string
#include <vector>

#include "unit_util.h"

template <typename K>
void write_soup_to_obj(const std::vector<typename K::Point_3>& points,
                       const std::vector<std::array<std::size_t, 3>>& triangles,
                       const std::string& filename) {
  std::ofstream outfile(filename);
  if (!outfile.is_open()) {
    std::cerr << "ERROR: Could not open " << filename << " for writing."
              << std::endl;
    return;
  }
  outfile << std::fixed << std::setprecision(16);
  CGAL::IO::write_OBJ(outfile, points, triangles);
  outfile.close();
  if (outfile.fail()) {
    std::cerr << "ERROR: Failed to write OBJ data to " << filename << "."
              << std::endl;
  } else {
    std::cout << "  Soup written to " << filename << std::endl;
  }
}

namespace PMP = CGAL::Polygon_mesh_processing;

template <typename K>
static bool autorefine_soup(
    std::vector<typename K::Point_3>& soup_points,
    std::vector<std::array<std::size_t, 3>>& soup_triangles) {
  const auto& snap_rounding_option =
      CGAL::parameters::apply_iterative_snap_rounding(true);
  bool success_autorefine =
      CGAL::Polygon_mesh_processing::autorefine_triangle_soup(
          soup_points, soup_triangles, snap_rounding_option);

  if (!success_autorefine) {
    std::cerr << "ERROR: autorefine_soup failed." << std::endl;
  }
  return success_autorefine;
}

template <typename K>
bool separate_non_manifold_vertices_in_soup(
    std::vector<typename K::Point_3>& soup_points,
    std::vector<std::array<std::size_t, 3>>& soup_triangles,
    double separation_delta) {
  bool changed = false;

  const std::size_t original_soup_points_size = soup_points.size();

  // 1. Build Adjacency Map: Map each vertex index to a list of triangle indices
  // that use it.
  //    This helps find all triangles incident to a given vertex.
  std::vector<std::vector<std::size_t>> vertex_to_triangle_map(
      original_soup_points_size);
  for (std::size_t tri_idx = 0; tri_idx < soup_triangles.size(); ++tri_idx) {
    for (std::size_t i = 0; i < 3; ++i) {
      std::size_t v_idx = soup_triangles[tri_idx][i];
      // Only map to original vertices. New vertices from duplication will not
      // be in this map yet.
      if (v_idx < original_soup_points_size) {
        vertex_to_triangle_map[v_idx].push_back(tri_idx);
      } else {
        // This case can happen if a previous duplication changed an index to a
        // new_v_idx. These new_v_idx don't need to be processed for
        // non-manifoldness in this pass. std::cerr << "LOG: WARNING: Triangle "
        // << tri_idx << " references new vertex index " << v_idx << ". Skipping
        // mapping." << std::endl;
      }
    }
  }

  // 2. Iterate Through Vertices:
  //    For each vertex index (0 to original_soup_points_size - 1):
  for (std::size_t v_idx = 0; v_idx < original_soup_points_size; ++v_idx) {
    const std::vector<std::size_t>& incident_tri_indices =
        vertex_to_triangle_map[v_idx];

    // If no triangles incident, it's an isolated vertex (not non-manifold in
    // this context)
    if (incident_tri_indices.empty()) {
      continue;
    }

    // 3. Analyze Local Connectivity (Identify "Fans"):
    //    For the current vertex:
    std::vector<bool> incident_tri_visited(incident_tri_indices.size(), false);
    std::vector<std::vector<std::size_t>>
        fans;  // Stores groups of triangle indices forming a fan

    // New logic to build local graph of incident triangles around v_idx (its
    // link) Nodes in this local graph are indices into incident_tri_indices
    std::vector<std::vector<std::size_t>> local_adj_graph(
        incident_tri_indices.size());

    // Map v_other (a vertex adjacent to v_idx in a triangle) to local incident
    // triangle indices This helps find triangles connected through a shared
    // edge (v_idx, v_other) Key: v_other vertex index Value: vector of local
    // indices of incident triangles that contain the edge (v_idx, v_other)
    std::map<std::size_t, std::vector<std::size_t>>
        v_other_to_incident_tri_local_idx;

    // First pass: populate v_other_to_incident_tri_local_idx
    for (std::size_t local_idx_i = 0; local_idx_i < incident_tri_indices.size();
         ++local_idx_i) {
      std::size_t tri_global_idx_i = incident_tri_indices[local_idx_i];
      const auto& tri_i = soup_triangles[tri_global_idx_i];

      // Find the two vertices of tri_i that are NOT v_idx
      for (int l = 0; l < 3; ++l) {
        if (tri_i[l] == v_idx) {
          // The two other vertices form edges with v_idx
          std::size_t v_other1 = tri_i[(l + 1) % 3];
          std::size_t v_other2 = tri_i[(l + 2) % 3];
          v_other_to_incident_tri_local_idx[v_other1].push_back(local_idx_i);
          v_other_to_incident_tri_local_idx[v_other2].push_back(local_idx_i);
          break;  // Found v_idx, so other_verts are (l+1)%3 and (l+2)%3
        }
      }
    }

    // Second pass: build local_adj_graph from v_other_to_incident_tri_local_idx
    // All triangles sharing a common v_other (i.e., a common edge with v_idx)
    // are connected
    for (const auto& entry : v_other_to_incident_tri_local_idx) {
      const std::vector<std::size_t>& connected_tri_local_indices =
          entry.second;
      for (std::size_t i = 0; i < connected_tri_local_indices.size(); ++i) {
        for (std::size_t j = i + 1; j < connected_tri_local_indices.size();
             ++j) {
          std::size_t t_local_idx1 = connected_tri_local_indices[i];
          std::size_t t_local_idx2 = connected_tri_local_indices[j];
          // Add connection in both directions (avoid duplicates)
          bool already_connected1 = false;
          for (std::size_t neighbor : local_adj_graph[t_local_idx1]) {
            if (neighbor == t_local_idx2) {
              already_connected1 = true;
              break;
            }
          }
          if (!already_connected1) {
            local_adj_graph[t_local_idx1].push_back(t_local_idx2);
          }

          bool already_connected2 = false;
          for (std::size_t neighbor : local_adj_graph[t_local_idx2]) {
            if (neighbor == t_local_idx1) {
              already_connected2 = true;
              break;
            }
          }
          if (!already_connected2) {
            local_adj_graph[t_local_idx2].push_back(t_local_idx1);
          }
        }
      }
    }

    // Now perform BFS/DFS on local_adj_graph to find connected components
    // (fans)
    for (std::size_t i = 0; i < incident_tri_indices.size(); ++i) {
      if (incident_tri_visited[i]) {
        continue;
      }

      // Start a new fan
      std::vector<std::size_t> current_fan;
      std::vector<std::size_t>
          q;  // Queue for breadth-first-search like traversal

      // Add the current triangle's global index to the fan
      current_fan.push_back(incident_tri_indices[i]);
      // Add its local index to the BFS queue
      q.push_back(i);
      incident_tri_visited[i] = true;

      std::size_t head = 0;
      while (head < q.size()) {
        std::size_t current_tri_local_idx = q[head++];

        // Explore neighbors in the local graph
        for (std::size_t neighbor_local_idx :
             local_adj_graph[current_tri_local_idx]) {
          if (!incident_tri_visited[neighbor_local_idx]) {
            incident_tri_visited[neighbor_local_idx] = true;
            current_fan.push_back(incident_tri_indices[neighbor_local_idx]);
            q.push_back(neighbor_local_idx);
          }
        }
      }

      // Add current_fan to fans after tracing is complete
      fans.push_back(current_fan);

    }  // End of fan tracing for the current vertex

    // 4. Perform Vertex Duplication:
    //    If the vertex is non-manifold (i.e., its incident triangles form N > 1
    //    distinct fans/sheets):
    if (fans.size() > 1) {
      changed = true;  // Mark as changed because we are performing separations
      // For each subsequent "fan" (N-1 times):
      for (std::size_t fan_idx = 1; fan_idx < fans.size(); ++fan_idx) {
        const std::vector<std::size_t>& current_fan_triangles = fans[fan_idx];

        // Calculate the average normal for the current fan
        typename K::Vector_3 average_normal(0, 0, 0);
        if (!current_fan_triangles.empty()) {
          for (std::size_t global_tri_idx : current_fan_triangles) {
            const auto& tri = soup_triangles[global_tri_idx];
            // Get vertex points for the triangle
            typename K::Point_3 p0 = soup_points[tri[0]];
            typename K::Point_3 p1 = soup_points[tri[1]];
            typename K::Point_3 p2 = soup_points[tri[2]];

            // Calculate triangle normal
            typename K::Vector_3 v1 = p1 - p0;
            typename K::Vector_3 v2 = p2 - p0;
            typename K::Vector_3 normal = CGAL::cross_product(v1, v2);

            // Accumulate normal (don't normalize individual triangle normals
            // yet) Handle degenerate triangles: if normal is zero vector, skip
            // it.
            if (normal.squared_length() > typename K::FT(0)) {
              average_normal = average_normal + normal;
            }
          }
          // Normalize the accumulated normal
          if (average_normal.squared_length() > typename K::FT(0)) {
            average_normal =
                average_normal /
                std::sqrt(CGAL::to_double(average_normal.squared_length()));
          }
        }

        // Use a small separation delta

        // Create a new point in 'soup_points' with the same coordinates as the
        // original vertex, plus a small offset along the average normal.
        std::size_t new_v_idx =
            soup_points.size();  // New index will be current size
        soup_points.push_back(soup_points[v_idx] +
                              (average_normal * separation_delta));

        // Update the 'soup_triangles' for all triangles belonging to this
        // particular "fan" to point to 'new_v_idx' instead of the original
        // vertex index.
        for (std::size_t global_tri_idx : current_fan_triangles) {
          // Basic bounds check for global_tri_idx
          if (global_tri_idx >= soup_triangles.size()) {
            // Handle error
            continue;
          }
          auto& tri = soup_triangles[global_tri_idx];
          for (std::size_t i = 0; i < 3; ++i) {
            if (tri[i] == v_idx) {
              tri[i] = new_v_idx;
            }
          }
        }
      }
    }
  }  // End of loop through vertices

  return changed;
}  // CLOSING BRACE ADDED HERE

template <typename K>
void separate_horn_structure_vertices_in_soup(
    std::vector<typename K::Point_3>& soup_points,
    std::vector<std::array<std::size_t, 3>>& soup_triangles,
    const std::vector<std::pair<std::size_t, std::size_t>>&
        intersecting_triangle_pairs) {
  // Iterate through each pair of intersecting triangles
  for (const auto& pair : intersecting_triangle_pairs) {
    std::size_t tri_idx1 = pair.first;
    std::size_t tri_idx2 = pair.second;

    const auto& tri1 = soup_triangles[tri_idx1];
    const auto& tri2 = soup_triangles[tri_idx2];

    // Find common vertices between tri1 and tri2
    std::set<std::size_t> common_vertices;
    for (std::size_t v1_idx : tri1) {
      for (std::size_t v2_idx : tri2) {
        if (v1_idx == v2_idx) {
          common_vertices.insert(v1_idx);
        }
      }
    }

    // A horn structure typically implies one common vertex or an edge
    // If there's one common vertex, that's likely the "tip"
    if (common_vertices.size() == 1) {
      std::size_t tip_v_idx = *common_vertices.begin();
      // ... Logic to separate this tip ...

      // Create a new point for the tip
      std::size_t new_tip_v_idx = soup_points.size();
      soup_points.push_back(soup_points[tip_v_idx]);

      // Update one of the triangles to use the new tip vertex
      // For simplicity, update tri_idx2 to use the new_tip_v_idx instead of
      // tip_v_idx
      auto& triangle_to_update = soup_triangles[tri_idx2];
      for (std::size_t& v_idx_in_tri : triangle_to_update) {
        if (v_idx_in_tri == tip_v_idx) {
          v_idx_in_tri = new_tip_v_idx;

          break;  // Only replace the first occurrence (should only be one for a
                  // vertex)
        }
      }
    } else if (common_vertices.size() == 2) {
      // Common edge, might need different handling or might not be a "horn tip"
      // ... Logic for common edge ...
    }
  }
}

template <typename K>
static bool remove_overlapping_geometry_in_soup(
    std::vector<typename K::Point_3>& soup_points,
    std::vector<std::array<std::size_t, 3>>& soup_triangles,
    std::vector<typename K::Point_3>& removed_points_out,
    std::vector<std::array<std::size_t, 3>>& removed_triangles_out,
    std::vector<typename K::Point_3>& even_winding_points_out,
    std::vector<std::array<std::size_t, 3>>& even_winding_triangles_out,
    std::vector<typename K::Point_3>& odd_winding_points_out,
    std::vector<std::array<std::size_t, 3>>& odd_winding_triangles_out) {
  if (soup_triangles.empty()) {
    return false;
  }

  using EPECK = CGAL::Exact_predicates_exact_constructions_kernel;
  using Point_3 = EPECK::Point_3;
  using Triangle_3 = EPECK::Triangle_3;
  using Iterator = std::vector<Triangle_3>::iterator;
  using Primitive = CGAL::AABB_triangle_primitive_3<EPECK, Iterator>;
  using Traits = CGAL::AABB_traits_3<EPECK, Primitive>;
  using AABB_tree = CGAL::AABB_tree<Traits>;

  // 1. Convert points to EPECK
  CGAL::Cartesian_converter<K, EPECK> to_epeck;
  std::vector<Point_3> points;
  points.reserve(soup_points.size());
  for (const auto& p : soup_points) {
    points.push_back(to_epeck(p));
  }

  // 2. Convert indices to actual Triangle objects for the AABB tree
  std::vector<Triangle_3> triangles;
  triangles.reserve(soup_triangles.size());
  for (const auto& t : soup_triangles) {
    triangles.emplace_back(points[t[0]], points[t[1]], points[t[2]]);
  }

  // 3. Build the AABB Tree
  AABB_tree tree(triangles.begin(), triangles.end());
  tree.accelerate_distance_queries();  // Optional: helps with speed

  std::vector<bool> is_internal(soup_triangles.size(), false);
  bool changed = false;

  for (size_t i = 0; i < triangles.size(); ++i) {
    Point_3 c = CGAL::centroid(triangles[i]);

    // Cast a ray from the centroid along the triangle's normal
    const Triangle_3& current_triangle_cgal = triangles[i];
    EPECK::Vector_3 e1 =
        current_triangle_cgal.vertex(1) - current_triangle_cgal.vertex(0);
    EPECK::Vector_3 e2 =
        current_triangle_cgal.vertex(2) - current_triangle_cgal.vertex(0);
    EPECK::Vector_3 raw_normal = CGAL::cross_product(e1, e2);

    // Using raw_normal directly as ray direction, assuming non-degenerate
    // triangles are handled elsewhere.
    CGAL::Ray_3<EPECK> ray(c, raw_normal);

    // An odd number of intersections means the point is inside the mesh (for a
    // closed manifold)
    std::vector<std::pair<CGAL::Object, typename AABB_tree::Primitive_id>>
        intersections_and_ids;
    tree.all_intersections(ray, std::back_inserter(intersections_and_ids));

    int valid_intersections_count = 0;
    std::vector<long> intersected_primitive_indices;

    // Filter out the self-intersection with the primitive from which the ray
    // originates
    for (const auto& p : intersections_and_ids) {
      long intersected_triangle_idx =
          std::distance(triangles.begin(), p.second);
      if (intersected_triangle_idx !=
          i) {  // Ignore intersection with the originating triangle
        valid_intersections_count++;
        intersected_primitive_indices.push_back(intersected_triangle_idx);
      }
    }

    if (valid_intersections_count % 2 == 1) {
      is_internal[i] = true;
      changed = true;
    }
    // Log ray intersections for debugging
  }

  // Populate output parameters
  std::map<size_t, size_t> old_to_new_even_map;
  std::map<size_t, size_t> old_to_new_odd_map;
  std::map<size_t, size_t>
      old_to_new_removed_map;  // For the actual removed geometry

  for (size_t i = 0; i < soup_triangles.size(); ++i) {
    std::array<std::size_t, 3> current_tri = soup_triangles[i];
    if (!is_internal[i]) {  // Even winding
      std::array<std::size_t, 3> new_tri;
      for (int k = 0; k < 3; ++k) {
        size_t old_idx = current_tri[k];
        if (old_to_new_even_map.find(old_idx) == old_to_new_even_map.end()) {
          old_to_new_even_map[old_idx] = even_winding_points_out.size();
          even_winding_points_out.push_back(soup_points[old_idx]);
        }
        new_tri[k] = old_to_new_even_map[old_idx];
      }
      even_winding_triangles_out.push_back(new_tri);
    } else {  // Odd winding (internal)
      std::array<std::size_t, 3> new_tri;
      for (int k = 0; k < 3; ++k) {
        size_t old_idx = current_tri[k];
        if (old_to_new_odd_map.find(old_idx) == old_to_new_odd_map.end()) {
          old_to_new_odd_map[old_idx] = odd_winding_points_out.size();
          odd_winding_points_out.push_back(soup_points[old_idx]);
        }
        new_tri[k] = old_to_new_odd_map[old_idx];
      }
      odd_winding_triangles_out.push_back(new_tri);

      // Also populate removed_points_out and removed_triangles_out
      std::array<std::size_t, 3> new_removed_tri;
      for (int k = 0; k < 3; ++k) {
        size_t old_idx = current_tri[k];
        if (old_to_new_removed_map.find(old_idx) ==
            old_to_new_removed_map.end()) {
          old_to_new_removed_map[old_idx] = removed_points_out.size();
          removed_points_out.push_back(soup_points[old_idx]);
        }
        new_removed_tri[k] = old_to_new_removed_map[old_idx];
      }
      removed_triangles_out.push_back(new_removed_tri);
    }
  }

  if (!changed)
    return true;  // Function successfully processed, no changes needed.

  // Filter soup
  std::vector<std::array<std::size_t, 3>> new_triangles;
  new_triangles.reserve(soup_triangles.size());
  for (size_t i = 0; i < soup_triangles.size(); ++i) {
    if (!is_internal[i]) {
      new_triangles.push_back(soup_triangles[i]);
    }
  }
  soup_triangles = new_triangles;

  // Remove unreferenced vertices
  std::map<size_t, size_t> old_to_new;
  std::vector<typename K::Point_3> new_points;
  for (auto& t : soup_triangles) {
    for (int k = 0; k < 3; ++k) {
      size_t old_idx = t[k];
      if (old_to_new.find(old_idx) == old_to_new.end()) {
        old_to_new[old_idx] = new_points.size();
        new_points.push_back(soup_points[old_idx]);
      }
      t[k] = old_to_new[old_idx];
    }
  }
  soup_points = new_points;

  return true;
}

template <typename K>
static bool fix_soup(
    std::vector<typename K::Point_3>& points,
    std::vector<std::array<std::size_t, 3>>& triangles,
    std::vector<typename K::Point_3>& out_removed_points,
    std::vector<std::array<std::size_t, 3>>& out_removed_triangles,
    std::vector<typename K::Point_3>& out_even_points,
    std::vector<std::array<std::size_t, 3>>& out_even_triangles,
    std::vector<typename K::Point_3>& out_odd_points,
    std::vector<std::array<std::size_t, 3>>& out_odd_triangles,
    double separation_delta = 0.05) {
  int max_iterations = 10;
  int i = 0;
  for (; i < max_iterations; ++i) {
    if (!autorefine_soup<K>(points, triangles)) {
      throw std::runtime_error("fix_soup: autorefine_soup failed.");
    }
    bool changed = separate_non_manifold_vertices_in_soup<K>(points, triangles,
                                                             separation_delta);
    if (!changed) break;
  }

  if (i == max_iterations) {
    throw std::runtime_error(
        "fix_soup: max iterations reached in separation loop");
  }

  return remove_overlapping_geometry_in_soup<K>(
      points, triangles, out_removed_points, out_removed_triangles,
      out_even_points, out_even_triangles, out_odd_points, out_odd_triangles);
}

template <typename K>
static void repair_soup(std::vector<typename K::Point_3>& vertices,
                        std::vector<std::vector<std::size_t>>& triangles,
                        double separation_delta = 0.05) {
  std::vector<std::array<std::size_t, 3>> fixed_triangles;
  fixed_triangles.reserve(triangles.size());
  for (const auto& t : triangles) {
    if (t.size() != 3) {
      // If we encounter non-triangular faces, we cannot proceed with fix_soup
      // which expects triangles. Assert or throw an error.
      // For now, let's assert. A proper solution might involve triangulating
      // first.
      assert(t.size() == 3 && "repair_soup expects only triangular faces.");
    }
    fixed_triangles.push_back({t[0], t[1], t[2]});
  }

  // These vectors are required by fix_soup signature but their content might
  // not be directly used outside this function if we only care about the
  // repaired main soup.
  std::vector<typename K::Point_3> removed_points;
  std::vector<std::array<std::size_t, 3>> removed_triangles;
  std::vector<typename K::Point_3> even_points;
  std::vector<std::array<std::size_t, 3>> even_triangles;
  std::vector<typename K::Point_3> odd_points;
  std::vector<std::array<std::size_t, 3>> odd_triangles;

  // Call fix_soup
  fix_soup<K>(vertices, fixed_triangles, removed_points, removed_triangles,
              even_points, even_triangles, odd_points, odd_triangles,
              separation_delta);

  // Convert back from fixed_triangles to original format
  triangles.clear();
  triangles.reserve(fixed_triangles.size());
  for (const auto& t : fixed_triangles) {
    triangles.push_back({t[0], t[1], t[2]});
  }
}
