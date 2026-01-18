#pragma once

#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/intersections.h>

#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "shape.h"
#include "sweep_util.h"
#include "validate_util.h"

namespace geometry {

template <typename K>
static GeometryId Sweep(Assets& assets, std::vector<Shape>& profile_shapes,
                        Shape& path_shape, bool closed_path,
                        bool closed_profile, int strategy_int, bool solid,
                        double pivot_offset_override, double min_turn_radius,
                        std::vector<std::string>* error_tokens = nullptr) {
  if (profile_shapes.empty())
    throw std::runtime_error("Sweep requires a profile.");

  SweepStrategy strategy = static_cast<SweepStrategy>(strategy_int);

  // 1. Collect Profile Data
  Geometry profile_combined;
  for (auto& shape : profile_shapes) {
    if (!shape.HasGeometryId()) continue;
    Geometry geometry =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    std::map<size_t, size_t> v_map;
    for (size_t i = 0; i < geometry.vertices_.size(); ++i) {
      const auto& v = geometry.vertices_[i];
      v_map[i] = profile_combined.AddVertex(v);
    }
    for (const auto& p : geometry.points_)
      profile_combined.points_.push_back(v_map[p]);
    for (const auto& s : geometry.segments_)
      profile_combined.segments_.emplace_back(v_map[s.first], v_map[s.second]);
    for (const auto& t : geometry.triangles_) {
      std::vector<size_t> tri;
      for (auto v : t) tri.push_back(v_map[v]);
      profile_combined.triangles_.push_back(tri);
    }
  }

  // 2. Identify Boundary Edges
  std::vector<std::pair<size_t, size_t>> edges_to_bridge;
  if (!profile_combined.triangles_.empty()) {
    std::map<std::pair<size_t, size_t>, int> edge_count;
    for (const auto& t : profile_combined.triangles_) {
      for (int i = 0; i < 3; ++i) {
        size_t v1 = t[i];
        size_t v2 = t[(i + 1) % 3];
        edge_count[{v1, v2}]++;
        edge_count[{v2, v1}]--;  // Net count for internal edges is 0
      }
    }
    for (const auto& [edge, count] : edge_count) {
      if (count == 1) edges_to_bridge.push_back(edge);
    }
  } else {
    edges_to_bridge = profile_combined.segments_;
  }

  // 3. Extract Path points
  std::vector<typename K::Point_3> path;
  path_shape.Walk([&](Shape& s) {
    if (s.HasGeometryId()) {
      Geometry geometry =
          assets.GetGeometry(s.GeometryId()).Transform(s.GetTf());
      if (!geometry.segments_.empty()) {
        // Follow segments to ensure correct order
        std::map<size_t, std::vector<size_t>> adj;
        for (const auto& seg : geometry.segments_) {
          adj[seg.first].push_back(seg.second);
          adj[seg.second].push_back(seg.first);
        }
        std::set<size_t> visited;
        std::vector<size_t> starts;
        for (size_t n = 0; n < geometry.vertices_.size(); ++n)
          if (adj[n].size() == 1) starts.push_back(n);
        if (starts.empty() && !geometry.vertices_.empty()) starts.push_back(0);

        for (size_t curr : starts) {
          if (visited.count(curr)) continue;
          while (true) {
            visited.insert(curr);
            CGAL::Cartesian_converter<EK, K> to_K;
            path.push_back(to_K(geometry.vertices_[curr]));
            size_t next = -1;
            for (size_t neighbor : adj[curr])
              if (!visited.count(neighbor)) {
                next = neighbor;
                break;
              }
            if (next == -1) break;
            curr = next;
          }
        }
      } else {
        for (const auto& v : geometry.vertices_) {
          CGAL::Cartesian_converter<EK, K> to_K;
          path.push_back(to_K(v));
        }
      }
    }
    return true;
  });
  if (path.size() < 2)
    throw std::runtime_error("Path must have at least 2 points.");

  // 4. Generate Frames
  std::vector<Frame> frames = generate_frames(path, closed_path);

  struct Shift {
    double x;
    double y;
  };
  std::vector<Shift> profile_shifts(frames.size(), {0.0, 0.0});

  for (size_t i = 0; i < frames.size(); ++i) {
    if (!closed_path && (i == 0 || i == frames.size() - 1)) continue;

    const auto& frame = frames[i];
    Frame F_prev = (i == 0) ? frames.back() : frames[i - 1];

    EK::Vector_3 t_out = frame.tangent;
    EK::Vector_3 t_in = F_prev.tangent;
    EK::Vector_3 bisector = t_out - t_in;
    double b_len_sq = CGAL::to_double(bisector.squared_length());

    if (b_len_sq > 1e-9) {
      EK::Vector_3 unit_bisector = bisector / std::sqrt(b_len_sq);
      double pivot_offset = 0;
      if (pivot_offset_override >= 0) {
        pivot_offset = pivot_offset_override;
      } else {
        double max_proj = -1e18;
        auto update_max_proj = [&](const EK::Vector_3& n,
                                   const EK::Vector_3& b) {
          for (const auto& v : profile_combined.vertices_) {
            double proj =
                CGAL::to_double((v.x() * n + v.y() * b) * unit_bisector);
            if (proj > max_proj) max_proj = proj;
          }
        };
        update_max_proj(F_prev.normal, F_prev.binormal);
        update_max_proj(frame.normal, frame.binormal);
        pivot_offset = max_proj + min_turn_radius;
      }
      EK::Vector_3 p_shift = -pivot_offset * unit_bisector;
      profile_shifts[i] = {CGAL::to_double(p_shift * frame.normal),
                           CGAL::to_double(p_shift * frame.binormal)};
    }
  }

  if (!closed_path && frames.size() >= 2) {
    profile_shifts[0] = profile_shifts[1];
    profile_shifts.back() = profile_shifts[profile_shifts.size() - 2];
  }

  Geometry output;

  // 5. Generate Slices
  std::vector<std::vector<size_t>> vertex_grid;
  struct JointSlices {
    size_t in;
    size_t out;
  };
  std::vector<JointSlices> joint_grid;
  std::optional<EK::Point_3> last_path;
  std::optional<EK::Point_3> last_pivot;

  auto add_safe_triangle = [&](size_t a, size_t b, size_t c) {
    if (a == b || b == c || c == a) return;
    output.AddTriangle(a, b, c);
  };

  auto bridge = [&](size_t idx_a, size_t idx_b) {
    if (idx_a == idx_b) return;
    // Longitudinal lines
    for (size_t p_idx : profile_combined.points_)
      output.segments_.emplace_back(vertex_grid[idx_a][p_idx],
                                    vertex_grid[idx_b][p_idx]);

    if (!solid) {
      // Wireframe: Add cross-section segments for idx_a
      for (const auto& s : profile_combined.segments_) {
        output.segments_.emplace_back(vertex_grid[idx_a][s.first],
                                      vertex_grid[idx_a][s.second]);
      }
      return;
    }

    for (const auto& s : edges_to_bridge) {
      size_t v00 = vertex_grid[idx_a][s.first];
      size_t v01 = vertex_grid[idx_a][s.second];
      size_t v10 = vertex_grid[idx_b][s.first];
      size_t v11 = vertex_grid[idx_b][s.second];
      // Outward CCW winding for CCW profiles: (v00, v01, v11) and (v00, v11,
      // v10)
      add_safe_triangle(v00, v01, v11);
      add_safe_triangle(v00, v11, v10);
    }
  };

  for (size_t i = 0; i < frames.size(); ++i) {
    const auto& frame = frames[i];
    Frame F_prev =
        (i == 0) ? (closed_path ? frames.back() : frames[0]) : frames[i - 1];

    EK::Vector_3 t_out = frame.tangent;
    EK::Vector_3 t_in = F_prev.tangent;
    EK::Vector_3 bisector = t_out - t_in;
    double b_len_sq = CGAL::to_double(bisector.squared_length());

    double dot_val = CGAL::to_double(CGAL::scalar_product(t_in, t_out));
    if (dot_val > 1.0) dot_val = 1.0;
    if (dot_val < -1.0) dot_val = -1.0;
    double theta = std::acos(dot_val);

    EK::Vector_3 profile_shift = profile_shifts[i].x * frame.normal +
                                 profile_shifts[i].y * frame.binormal;

    // The rotation pivot is offset INWARD from the path.
    EK::Point_3 pivot_pos = frame.position - profile_shift;
    EK::Point_3 path_pos = frame.position;

    for (int offset_z = 0; offset_z <= 5; ++offset_z) {
      auto offset_vec = EK::Vector_3(0, 0, offset_z);
      if (last_path) {
        output.AddSegment(*last_path + offset_vec, path_pos + offset_vec);
      }
      if (last_pivot) {
        output.AddSegment(*last_pivot + offset_vec, pivot_pos + offset_vec);
      }
    }
    last_path = path_pos;
    last_pivot = pivot_pos;

    EK::Point_3 pivot = frame.position;

    if (strategy != SweepStrategy::ROUND) {
      throw std::runtime_error(
          "Only ROUND sweep strategy is currently supported.");
    }

    if (theta < 1e-6 || b_len_sq < 1e-9) {
      std::vector<size_t> slice;
      for (const auto& p : profile_combined.vertices_)
        slice.push_back(output.AddVertex(frame.transform(p)));
      vertex_grid.push_back(slice);
      size_t idx = vertex_grid.size() - 1;
      joint_grid.push_back({idx, idx});
    } else {
      EK::Vector_3 axis = CGAL::cross_product(t_out, t_in);

      int num_round_slices = 8;
      size_t first_idx = 0;
      size_t last_slice_idx = 0;
      bool joint_first = true;
      for (int s = 0; s <= num_round_slices; ++s) {
        double t = (double)s / num_round_slices;

        auto transform_rigid = [&](const EK::Point_3& p) {
          // Profile at frame orientation, centered on path
          EK::Point_3 p_world = frame.position +
                                CGAL::to_double(p.x()) * frame.normal +
                                CGAL::to_double(p.y()) * frame.binormal;
          // Rotate centered profile around the inward-offset pivot
          return pivot_rotate(p_world, pivot_pos, axis, theta * (1.0 - t));
        };

        std::vector<size_t> slice;
        for (const auto& p : profile_combined.vertices_) {
          slice.push_back(output.AddVertex(transform_rigid(p)));
        }
        vertex_grid.push_back(slice);
        size_t current_idx = vertex_grid.size() - 1;
        if (joint_first) {
          first_idx = current_idx;
          joint_first = false;
        } else {
          bridge(last_slice_idx, current_idx);
        }
        last_slice_idx = current_idx;
      }
      joint_grid.push_back({first_idx, last_slice_idx});
    }
  }

  for (size_t i = 0; i < frames.size() - 1; ++i)

    bridge(joint_grid[i].out, joint_grid[i + 1].in);

  if (closed_path) bridge(joint_grid.back().out, joint_grid[0].in);

  if (!solid && !closed_path && !profile_combined.segments_.empty()) {
    size_t end_idx = joint_grid.back().out;

    for (const auto& s : profile_combined.segments_) {
      output.segments_.emplace_back(vertex_grid[end_idx][s.first],

                                    vertex_grid[end_idx][s.second]);
    }
  }

  if (solid && !closed_path && !profile_combined.triangles_.empty()) {
    // Start cap

    for (const auto& t : profile_combined.triangles_) {
      output.AddTriangle(vertex_grid[joint_grid.front().in][t[0]],

                         vertex_grid[joint_grid.front().in][t[2]],

                         vertex_grid[joint_grid.front().in][t[1]]);
    }

    // End cap

    for (const auto& t : profile_combined.triangles_) {
      output.AddTriangle(vertex_grid[joint_grid.back().out][t[0]],

                         vertex_grid[joint_grid.back().out][t[1]],

                         vertex_grid[joint_grid.back().out][t[2]]);
    }
  }

  if (solid && !output.triangles_.empty()) {
    CGAL::Surface_mesh<typename K::Point_3> mesh;
    output.EncodeSurfaceMesh<K>(mesh);

    validate_mesh(mesh, "Sweep/Output", error_tokens);

    std::vector<
        std::pair<typename CGAL::Surface_mesh<typename K::Point_3>::Face_index,
                  typename CGAL::Surface_mesh<typename K::Point_3>::Face_index>>
        intersecting_pairs;

    CGAL::Polygon_mesh_processing::self_intersections(
        mesh, std::back_inserter(intersecting_pairs));

    if (!intersecting_pairs.empty()) {
      size_t interior_count = 0;
      size_t edge_count = 0;
      size_t vertex_count = 0;

      auto offset = EK::Vector_3(0, 0, 5);
      for (const auto& pair : intersecting_pairs) {
        auto h1 = mesh.halfedge(pair.first);
        EK::Triangle_3 tri1(mesh.point(mesh.source(h1)),
                            mesh.point(mesh.target(h1)),
                            mesh.point(mesh.target(mesh.next(h1))));

        auto h2 = mesh.halfedge(pair.second);
        EK::Triangle_3 tri2(mesh.point(mesh.source(h2)),
                            mesh.point(mesh.target(h2)),
                            mesh.point(mesh.target(mesh.next(h2))));

        auto result = CGAL::intersection(tri1, tri2);
        if (result) {
          if (const EK::Segment_3* s = std::get_if<EK::Segment_3>(&*result)) {
            // Check if segment endpoints are vertices of the triangles
            bool s_is_v = false;
            bool t_is_v = false;
            for (int i = 0; i < 3; ++i) {
              if (s->source() == tri1.vertex(i) ||
                  s->source() == tri2.vertex(i))
                s_is_v = true;
              if (s->target() == tri1.vertex(i) ||
                  s->target() == tri2.vertex(i))
                t_is_v = true;
            }

            if (s_is_v && t_is_v) {
              edge_count++;
            } else {
              interior_count++;
              output.AddSegment(s->source() + offset, s->target() + offset);
            }
          } else if (std::holds_alternative<std::vector<EK::Point_3>>(
                         *result) ||
                     std::holds_alternative<EK::Triangle_3>(*result)) {
            interior_count++;
            // Visualize complex intersections (polygons/triangles) as segments
            if (const auto* poly =
                    std::get_if<std::vector<EK::Point_3>>(&*result)) {
              for (size_t i = 0; i < poly->size(); ++i) {
                output.AddSegment((*poly)[i] + offset,
                                  (*poly)[(i + 1) % poly->size()] + offset);
              }
            } else if (const auto* t = std::get_if<EK::Triangle_3>(&*result)) {
              for (int i = 0; i < 3; ++i) {
                output.AddSegment(t->vertex(i) + offset,
                                  t->vertex((i + 1) % 3) + offset);
              }
            }
          } else if (std::holds_alternative<EK::Point_3>(*result)) {
            vertex_count++;
          }
        }
      }
      std::cout << "Sweep/Output: " << intersecting_pairs.size()
                << " total intersections (" << interior_count << " interior, "
                << edge_count << " edge-only, " << vertex_count
                << " vertex-only)." << std::endl;
    }

    size_t bowtie_count = 0;
    for (auto f : mesh.faces()) {
      auto h = mesh.halfedge(f);
      auto p0 = mesh.point(mesh.source(h));
      auto p1 = mesh.point(mesh.target(h));
      auto p2 = mesh.point(mesh.target(mesh.next(h)));
      if (CGAL::to_double(
              CGAL::cross_product(p1 - p0, p2 - p0).squared_length()) < 1e-12) {
        bowtie_count++;
        auto offset_bt = EK::Vector_3(0, 0, 10);
        output.AddSegment(p0 + offset_bt, p1 + offset_bt);
        output.AddSegment(p1 + offset_bt, p2 + offset_bt);
        output.AddSegment(p2 + offset_bt, p0 + offset_bt);
      }
    }
    if (bowtie_count > 0) {
      std::cout << "Sweep/Output: " << bowtie_count
                << " bowtie/degenerate triangles detected." << std::endl;
    }
  }

  return assets.TextId(output);
}

/*
 * Sweep Geometric Constraints:
 *
 * The `pivot_offset` parameter defines the offset distance from the path to the
 * center of rotation (pivot) used for generating rounded corners. To produce
 * valid, non-self-intersecting geometry, `pivot_offset` must satisfy the
 * condition:
 *
 *     ProfileRadius < pivot_offset < PathCurvatureRadius
 *
 * 1. If pivot_offset < ProfileRadius: The pivot falls inside the profile
 * geometry. Rotating the profile around this pivot causes the inner part of the
 * profile to move backwards relative to the sweep, creating a local
 * self-intersection (a "bowtie" artifact) within the corner fan.
 *
 * 2. If pivot_offset > PathCurvatureRadius: The offset path (the trajectory of
 * the pivot) forms a retrograde loop or cusp singularity at sharp corners. This
 * causes the entire corner assembly to fold back onto the previous segment,
 * creating overlapping geometry and "kinks" in the pivot line.
 *
 * If the profile is too wide for the tightness of the path's turns (i.e.,
 * ProfileRadius >= PathCurvatureRadius), no valid `pivot_offset` exists, and
 * self-intersection is geometrically inevitable for a rigid profile sweep.
 */

}  // namespace geometry