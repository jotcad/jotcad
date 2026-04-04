#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

#include <cmath>
#include <string>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "projection_util.h"
#include "shape.h"

namespace geometry {

struct HeightGrid {
  std::vector<std::vector<double>> h;
  double min_u, max_u, min_v, max_v;
  int rows, cols;

  double sample(double u, double v) const {
    if (h.empty()) return 0.0;
    double u_frac = (max_u > min_u) ? (u - min_u) / (max_u - min_u) : 0.5;
    double v_frac = (max_v > min_v) ? (v - min_v) / (max_v - min_v) : 0.5;

    double fi = u_frac * (rows - 1);
    double fj = v_frac * (cols - 1);

    int i0 = std::max(0, std::min(rows - 1, (int)std::floor(fi)));
    int j0 = std::max(0, std::min(cols - 1, (int)std::floor(fj)));
    int i1 = std::min(rows - 1, i0 + 1);
    int j1 = std::min(cols - 1, j0 + 1);

    double di = fi - i0;
    double dj = fj - j0;

    double v00 = h[i0][j0];
    double v10 = h[i1][j0];
    double v01 = h[i0][j1];
    double v11 = h[i1][j1];

    return (1.0 - di) * (1.0 - dj) * v00 + di * (1.0 - dj) * v10 +
           (1.0 - di) * dj * v01 + di * dj * v11;
  }
};

template <typename K>

static HeightGrid BinToHeightGrid(Assets& assets, std::vector<Shape>& shapes,

                                  int rows, int cols,

                                  const std::string& mapping = "planar",

                                  const std::string& strategy = "max",

                                  double min_u_override = 1e18,

                                  double max_u_override = -1e18,

                                  double min_v_override = 1e18,

                                  double max_v_override = -1e18) {
  std::vector<typename K::Point_3> cloud;

  for (auto& shape : shapes) {
    if (!shape.HasGeometryId()) continue;

    Geometry geom =

        assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

    CGAL::Cartesian_converter<EK, K> to_K;

    for (const auto& v : geom.vertices_) {
      cloud.push_back(to_K(v));
    }
  }

  HeightGrid grid;

  grid.rows = rows;

  grid.cols = cols;

  grid.min_u = min_u_override;

  grid.max_u = max_u_override;

  grid.min_v = min_v_override;

  grid.max_v = max_v_override;

  struct Projected {
    double u, v, h;
  };

  std::vector<Projected> projected;

  for (const auto& p : cloud) {
    double u, v, h;

    project<K>(p, mapping, u, v, h);

    if (u < grid.min_u) grid.min_u = u;

    if (u > grid.max_u) grid.max_u = u;

    if (v < grid.min_v) grid.min_v = v;

    if (v > grid.max_v) grid.max_v = v;

    projected.push_back({u, v, h});
  }

  grid.h.assign(rows, std::vector<double>(cols, 0.0));

  std::vector<std::vector<int>> count(rows, std::vector<int>(cols, 0));

  for (const auto& p : projected) {
    double uf = (grid.max_u > grid.min_u)
                    ? (p.u - grid.min_u) / (grid.max_u - grid.min_u)
                    : 0.0;

    double vf = (grid.max_v > grid.min_v)
                    ? (p.v - grid.min_v) / (grid.max_v - grid.min_v)
                    : 0.0;

    int i = std::max(0, std::min(rows - 1, (int)(uf * (rows - 1))));

    int j = std::max(0, std::min(cols - 1, (int)(vf * (cols - 1))));

    if (strategy == "max") {
      if (count[i][j] == 0 || p.h > grid.h[i][j]) {
        grid.h[i][j] = p.h;
      }

    } else {
      grid.h[i][j] += p.h;
    }

    count[i][j]++;
  }

  if (rows > 0 && cols > 0) {
    std::cout << "BinToHeightGrid: strategy=" << strategy << " u_range=["
              << grid.min_u << "," << grid.max_u << "] h[0][0]=" << grid.h[0][0]
              << std::endl;
  }

  if (strategy == "average") {
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        if (count[i][j] > 0) {
          grid.h[i][j] /= count[i][j];
        }
      }
    }
  }

  // Iterative gap fill to handle sparse data
  int total_bins = rows * cols;
  int initial_filled = 0;
  for (int i = 0; i < rows; ++i)
    for (int j = 0; j < cols; ++j)
      if (count[i][j] > 0) initial_filled++;

  std::cout << "BinToHeightGrid: Starting iterative fill. Initial: "
            << initial_filled << "/" << total_bins << " strategy=" << strategy
            << std::endl;

  // We loop until no more changes occur or a safety limit is reached.
  for (int pass = 0; pass < std::max(rows, cols); ++pass) {
    bool changed = false;
    std::vector<std::pair<int, int>> to_fill;
    std::vector<double> fill_values;

    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        if (count[i][j] == 0) {
          double sum = 0;
          double max_val = -1e18;
          int neighbors = 0;

          for (int di = -1; di <= 1; ++di) {
            for (int dj = -1; dj <= 1; ++dj) {
              if (di == 0 && dj == 0) continue;
              int ni = (i + di + rows) % rows;
              int nj = std::max(0, std::min(cols - 1, j + dj));

              if (count[ni][nj] > 0) {
                if (strategy == "max") {
                  if (grid.h[ni][nj] > max_val) max_val = grid.h[ni][nj];
                } else {
                  sum += grid.h[ni][nj];
                }
                neighbors++;
              }
            }
          }

          if (neighbors > 0) {
            to_fill.push_back({i, j});
            fill_values.push_back(strategy == "max" ? max_val
                                                    : (sum / neighbors));
            changed = true;
          }
        }
      }
    }

    if (!changed) {
      int final_filled = 0;
      for (int i = 0; i < rows; ++i)
        for (int j = 0; j < cols; ++j)
          if (count[i][j] > 0) final_filled++;
      std::cout << "BinToHeightGrid: Fill finished. Pass " << pass
                << " Final: " << final_filled << "/" << total_bins << std::endl;
      break;
    }

    for (size_t k = 0; k < to_fill.size(); ++k) {
      grid.h[to_fill[k].first][to_fill[k].second] = fill_values[k];
      count[to_fill[k].first][to_fill[k].second] = 1;  // Mark as filled
    }
  }
  return grid;
}

}  // namespace geometry
