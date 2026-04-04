#pragma once

#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

#include <cmath>
#include <string>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "grid_util.h"
#include "projection_util.h"
#include "shape.h"
#include "validate_util.h"

namespace geometry {

template <typename K>
static void make_planar_cutters(
    const CGAL::Bbox_3& bbox, const HeightGrid& grid,
    CGAL::Surface_mesh<typename K::Point_3>& mesh_u,
    CGAL::Surface_mesh<typename K::Point_3>& mesh_v) {
  double big_delta = 1.0;  // Margin
  double z_min = bbox.zmin() - big_delta;
  double z_max = bbox.zmax() + big_delta;
  double y_min = bbox.ymin() - big_delta;
  double y_max = bbox.ymax() + big_delta;
  double x_min = bbox.xmin() - big_delta;
  double x_max = bbox.xmax() + big_delta;

  // U-planes (X = constant)
  for (int i = 0; i < grid.rows; ++i) {
    double u = grid.min_u + (double)i / std::max(1, grid.rows - 1) *
                                (grid.max_u - grid.min_u);
    if (u < bbox.xmin() || u > bbox.xmax()) continue;

    auto v0 = mesh_u.add_vertex(typename K::Point_3(u, y_min, z_min));
    auto v1 = mesh_u.add_vertex(typename K::Point_3(u, y_max, z_min));
    auto v2 = mesh_u.add_vertex(typename K::Point_3(u, y_max, z_max));
    auto v3 = mesh_u.add_vertex(typename K::Point_3(u, y_min, z_max));
    mesh_u.add_face(v0, v1, v2, v3);
  }

  // V-planes (Y = constant)
  for (int j = 0; j < grid.cols; ++j) {
    double v = grid.min_v + (double)j / std::max(1, grid.cols - 1) *
                                (grid.max_v - grid.min_v);
    if (v < bbox.ymin() || v > bbox.ymax()) continue;

    auto v0 = mesh_v.add_vertex(typename K::Point_3(x_min, v, z_min));
    auto v1 = mesh_v.add_vertex(typename K::Point_3(x_max, v, z_min));
    auto v2 = mesh_v.add_vertex(typename K::Point_3(x_max, v, z_max));
    auto v3 = mesh_v.add_vertex(typename K::Point_3(x_min, v, z_max));
    mesh_v.add_face(v0, v1, v2, v3);
  }
}

template <typename K>
static void make_spherical_cutters(
    const CGAL::Bbox_3& bbox, const HeightGrid& grid,
    CGAL::Surface_mesh<typename K::Point_3>& mesh_u,
    CGAL::Surface_mesh<typename K::Point_3>& mesh_v) {
  double max_dim = std::max({std::abs(bbox.xmax()), std::abs(bbox.xmin()),
                             std::abs(bbox.ymax()), std::abs(bbox.ymin()),
                             std::abs(bbox.zmax()), std::abs(bbox.zmin())});
  double R = max_dim * 1.5;

  // U-planes (Longitude)
  for (int i = 0; i < grid.rows; ++i) {
    double u = grid.min_u + (double)i / std::max(1, grid.rows - 1) *
                                (grid.max_u - grid.min_u);

    double c = std::cos(u);
    double s = std::sin(u);

    auto v0 = mesh_u.add_vertex(typename K::Point_3(0, 0, -R));
    auto v1 = mesh_u.add_vertex(typename K::Point_3(R * c, R * s, -R));
    auto v2 = mesh_u.add_vertex(typename K::Point_3(R * c, R * s, R));
    auto v3 = mesh_u.add_vertex(typename K::Point_3(0, 0, R));
    mesh_u.add_face(v0, v1, v2, v3);
  }

  // V-cones (Latitude)
  for (int j = 0; j < grid.cols; ++j) {
    double v = grid.min_v + (double)j / std::max(1, grid.cols - 1) *
                                (grid.max_v - grid.min_v);

    if (v < 0.01 || v > M_PI - 0.01) continue;

    double z = R * std::cos(v);
    double r_ring = R * std::sin(v);

    int steps = 36;
    auto top = mesh_v.add_vertex(typename K::Point_3(0, 0, 0));
    std::vector<typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index>
        ring;

    for (int k = 0; k < steps; ++k) {
      double theta = 2.0 * M_PI * k / steps;
      ring.push_back(mesh_v.add_vertex(typename K::Point_3(
          r_ring * std::cos(theta), r_ring * std::sin(theta), z)));
    }

    for (int k = 0; k < steps; ++k) {
      mesh_v.add_face(top, ring[k], ring[(k + 1) % steps]);
    }
  }
}

template <typename K>
static GeometryId Texture(Assets& assets, Shape& base_shape,
                          std::vector<Shape>& texture_shapes, int rows,
                          int cols, const std::string& mapping, double scale,
                          const std::string& strategy = "max",
                          std::vector<std::string>* error_tokens = nullptr) {
  if (!base_shape.HasGeometryId())
    throw std::runtime_error("Texture requires a base shape with geometry.");

  // 1. Bin the texture geometry into a height grid
  // We assume the texture geometry is laid out along Z on the planar XY map.
  HeightGrid grid = BinToHeightGrid<K>(assets, texture_shapes, rows, cols,
                                       "planar", strategy);

  // 2. Prepare the base mesh
  Geometry output =
      assets.GetGeometry(base_shape.GeometryId()).Transform(base_shape.GetTf());

  if (output.triangles_.empty())
    throw std::runtime_error("Texture requires a mesh (triangles).");

  CGAL::Surface_mesh<typename K::Point_3> mesh;
  output.EncodeSurfaceMesh<K>(mesh);

  auto v_normals =
      mesh.template add_property_map<
              typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index,
              typename K::Vector_3>("v:normal")
          .first;
  CGAL::Polygon_mesh_processing::compute_vertex_normals(mesh, v_normals);

  // 3. Generate Cutter Meshes (U and V)
  CGAL::Surface_mesh<typename K::Point_3> mesh_u;
  CGAL::Surface_mesh<typename K::Point_3> mesh_v;
  auto bbox = CGAL::Polygon_mesh_processing::bbox(mesh);

  if (mapping == "planar") {
    make_planar_cutters<K>(bbox, grid, mesh_u, mesh_v);
  } else if (mapping == "spherical") {
    make_spherical_cutters<K>(bbox, grid, mesh_u, mesh_v);
  }
  // TODO: Cylindrical

  // 4. Corefine (Split edges)
  if (grid.rows > 1) {
    CGAL::Polygon_mesh_processing::corefine(mesh, mesh_u);
  }
  if (grid.cols > 1) {
    CGAL::Polygon_mesh_processing::corefine(mesh, mesh_v);
  }

  // 5. Re-compute normals after corefinement added new vertices/edges
  v_normals =
      mesh.template add_property_map<
              typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index,
              typename K::Vector_3>("v:normal")
          .first;
  CGAL::Polygon_mesh_processing::compute_vertex_normals(mesh, v_normals);

  // 6. Displace vertices along normals
  for (auto v : mesh.vertices()) {
    auto& p = mesh.point(v);
    double u_val, v_val, h_val;
    project<K>(p, mapping, u_val, v_val, h_val);

    double displacement = grid.sample(u_val, v_val) * scale;

    typename K::Vector_3 n = v_normals[v];
    double n_sq = CGAL::to_double(n.squared_length());
    if (n_sq > 1e-18) {
      n = n / std::sqrt(n_sq);
      p = p + n * displacement;
    }
  }

  Geometry result;
  result.DecodeSurfaceMesh<K>(mesh);
  return assets.TextId(result);
}

}  // namespace geometry