#pragma once

#include <CGAL/Cartesian_converter.h>

#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "assets.h"
#include "catmull_rom_util.h"
#include "geometry.h"
#include "remesh_util.h"
#include "shape.h"
#include "validate_util.h"

namespace geometry {

template <typename K>
static GeometryId Relief(Assets& assets, std::vector<Shape>& cloud_shapes,
                         int rows, int cols, const std::string& mapping,
                         double subdivisions, bool closed_u, bool closed_v,
                         double target_edge_length,
                         std::vector<std::string>* error_tokens = nullptr) {
  if (rows < 2 || cols < 2)
    throw std::runtime_error("Relief requires at least 2 rows and 2 columns.");

  // 1. Collect points from cloud
  std::vector<typename K::Point_3> cloud;
  for (auto& shape : cloud_shapes) {
    if (!shape.HasGeometryId()) continue;
    Geometry geom =
        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());
    CGAL::Cartesian_converter<EK, K> to_K;
    for (const auto& v : geom.vertices_) {
      cloud.push_back(to_K(v));
    }
  }

  if (cloud.empty()) throw std::runtime_error("Point cloud is empty.");

  // 2. Project and find UV bounds
  struct ProjectedPoint {
    double u, v, h;
    typename K::Point_3 original;
  };
  std::vector<ProjectedPoint> projected;
  double min_u = 1e18, max_u = -1e18, min_v = 1e18, max_v = -1e18;

  for (const auto& p : cloud) {
    double x = CGAL::to_double(p.x());
    double y = CGAL::to_double(p.y());
    double z = CGAL::to_double(p.z());
    double u, v, h;

    if (mapping == "spherical") {
      double r = std::sqrt(x * x + y * y + z * z);
      u = std::atan2(y, x);            // Longitude [-PI, PI]
      v = std::acos(z / (r + 1e-12));  // Latitude [0, PI]
      h = r;
    } else if (mapping == "cylindrical") {
      u = std::atan2(y, x);  // Angle [-PI, PI]
      v = z;                 // Height
      h = std::sqrt(x * x + y * y);
    } else {  // planar XY
      u = x;
      v = y;
      h = z;
    }

    if (u < min_u) min_u = u;
    if (u > max_u) max_u = u;
    if (v < min_v) min_v = v;
    if (v > max_v) max_v = v;
    projected.push_back({u, v, h, p});
  }

  // 3. Bin into grid
  // Note: rows = U (longitude), cols = V (latitude/height)
  std::vector<std::vector<double>> h_grid(rows, std::vector<double>(cols, 0.0));
  std::vector<std::vector<int>> count_grid(rows, std::vector<int>(cols, 0));

  for (const auto& p : projected) {
    double u_frac = (max_u > min_u) ? (p.u - min_u) / (max_u - min_u) : 0.0;
    double v_frac = (max_v > min_v) ? (p.v - min_v) / (max_v - min_v) : 0.0;

    int i = (int)(u_frac * (rows - 1));
    int j = (int)(v_frac * (cols - 1));

    if (i >= rows) i = rows - 1;
    if (j >= cols) j = cols - 1;

    h_grid[i][j] += p.h;
    count_grid[i][j]++;
  }

  // 4. Fill and generate 3D Control Grid
  std::vector<std::vector<typename K::Point_3>> control_grid(
      rows, std::vector<typename K::Point_3>(cols));
  for (int i = 0; i < rows; ++i) {
    for (int j = 0; j < cols; ++j) {
      double h = (count_grid[i][j] > 0) ? h_grid[i][j] / count_grid[i][j] : 0.0;

      // If h is 0, try to find a neighbor (simple gap fill)
      if (h == 0.0) {
        for (int di = -1; di <= 1; ++di) {
          for (int dj = -1; dj <= 1; ++dj) {
            int ni = (i + di + rows) % rows;
            int nj = std::max(0, std::min(cols - 1, j + dj));
            if (count_grid[ni][nj] > 0) {
              h = h_grid[ni][nj] / count_grid[ni][nj];
              goto found_h;
            }
          }
        }
      }
    found_h:

      double u_range = max_u - min_u;
      double v_range = max_v - min_v;

      double u = (closed_u && u_range > 0)
                     ? min_u + (double)i / rows * u_range
                     : min_u + (double)i / std::max(1, rows - 1) * u_range;
      double v = (closed_v && v_range > 0)
                     ? min_v + (double)j / cols * v_range
                     : min_v + (double)j / std::max(1, cols - 1) * v_range;

      if (mapping == "spherical") {
        // u = lon, v = lat (acos(z/r))
        control_grid[i][j] =
            typename K::Point_3(h * std::sin(v) * std::cos(u),
                                h * std::sin(v) * std::sin(u), h * std::cos(v));
      } else if (mapping == "cylindrical") {
        // u = angle, v = z, h = radius
        control_grid[i][j] =
            typename K::Point_3(h * std::cos(u), h * std::sin(u), v);
      } else {
        // u = x, v = y, h = z
        control_grid[i][j] = typename K::Point_3(u, v, h);
      }
    }
  }

  // 5. Bicubic Surface Interpolation
  auto get_padded = [&](int i, int j) {
    if (closed_u) {
      i = (i + rows) % rows;
    } else {
      if (i < 0) i = 0;
      if (i >= rows) i = rows - 1;
    }
    if (closed_v) {
      j = (j + cols) % cols;
    } else {
      if (j < 0) j = 0;
      if (j >= cols) j = cols - 1;
    }
    return control_grid[i][j];
  };

  int steps = (int)subdivisions;
  if (steps < 1) steps = 4;

  Geometry output;

  int sample_rows = (closed_u ? rows : rows - 1) * steps;

  if (!closed_u) sample_rows += 1;

  int sample_cols = (closed_v ? cols : cols - 1) * steps;

  if (!closed_v) sample_cols += 1;

  std::vector<std::vector<size_t>> vertex_indices(

      sample_rows, std::vector<size_t>(sample_cols));

  for (int si = 0; si < sample_rows; ++si) {
    double row_t = (double)si / steps;

    int i = (int)std::floor(row_t);

    double ti = row_t - i;

    for (int sj = 0; sj < sample_cols; ++sj) {
      double col_t = (double)sj / steps;

      int j = (int)std::floor(col_t);

      double tj = col_t - j;

      typename K::Point_3 row_pts[4];

      for (int r = 0; r < 4; ++r) {
        row_pts[r] = catmull_rom<K>(get_padded(i - 1 + r, j - 1),

                                    get_padded(i - 1 + r, j),

                                    get_padded(i - 1 + r, j + 1),

                                    get_padded(i - 1 + r, j + 2), tj);
      }

      typename K::Point_3 final_p =

          catmull_rom<K>(row_pts[0], row_pts[1], row_pts[2], row_pts[3], ti);

      vertex_indices[si][sj] = output.AddVertex(final_p);
    }
  }

  // 6. Generate Triangles

  for (int i = 0; i < sample_rows; ++i) {
    for (int j = 0; j < sample_cols; ++j) {
      if (!closed_u && i == sample_rows - 1) continue;

      if (!closed_v && j == sample_cols - 1) continue;

      size_t v00 = vertex_indices[i][j];

      size_t v01 = vertex_indices[i][(j + 1) % sample_cols];

      size_t v10 = vertex_indices[(i + 1) % sample_rows][j];

      size_t v11 = vertex_indices[(i + 1) % sample_rows][(j + 1) % sample_cols];

      output.AddTriangle(v00, v11, v01);

      output.AddTriangle(v00, v10, v11);
    }
  }

  CGAL::Surface_mesh<typename K::Point_3> mesh;
  output.EncodeSurfaceMesh<K>(mesh);

  if (target_edge_length > 0.0) {
    isotropic_remesh<K>(mesh, target_edge_length, 1);
    output.vertices_.clear();
    output.triangles_.clear();
    output.DecodeSurfaceMesh<K>(mesh);
  }

  validate_mesh(mesh, "Relief/Output", error_tokens);

  return assets.TextId(output);
}

}  // namespace geometry
