#pragma once

#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/distance.h>
#include <CGAL/Polygon_mesh_processing/fair.h>
#include <CGAL/Polygon_mesh_processing/refine.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/Euler_operations.h>
#include <CGAL/convex_hull_3.h>

#include <cmath>
#include <iostream>
#include <set>
#include <vector>

#include "assets.h"
#include "geometry.h"
#include "repair_util.h"
#include "shape.h"

namespace geometry {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK;

template <typename K>
static void create_tube_around_polylines(
    std::vector<Shape>& polylines, double radius, Assets& assets,
    CGAL::Surface_mesh<typename K::Point_3>& tube_mesh) {
  CGAL::Cartesian_converter<EK, K> to_K;
  for (auto& polyline : polylines) {
    if (!polyline.HasGeometryId()) continue;
    Geometry geom =
        assets.GetGeometry(polyline.GeometryId()).Transform(polyline.GetTf());

    for (const auto& segment : geom.segments_) {
      const auto p0 = to_K(geom.vertices_[segment.first]);
      const auto p1 = to_K(geom.vertices_[segment.second]);

      typename K::Vector_3 v = p1 - p0;
      if (v.squared_length() == 0) continue;

      // Construct a frame
      typename K::Vector_3 n1;
      if (std::abs(CGAL::to_double(v.x())) < std::abs(CGAL::to_double(v.y())) &&
          std::abs(CGAL::to_double(v.x())) < std::abs(CGAL::to_double(v.z()))) {
        n1 = CGAL::cross_product(v, typename K::Vector_3(1, 0, 0));
      } else if (std::abs(CGAL::to_double(v.y())) <
                 std::abs(CGAL::to_double(v.z()))) {
        n1 = CGAL::cross_product(v, typename K::Vector_3(0, 1, 0));
      } else {
        n1 = CGAL::cross_product(v, typename K::Vector_3(0, 0, 1));
      }
      typename K::Vector_3 n2 = CGAL::cross_product(v, n1);

      double len1 = CGAL::to_double(n1.squared_length());
      double len2 = CGAL::to_double(n2.squared_length());

      if (len1 > 0) n1 = n1 * (radius / std::sqrt(len1));
      if (len2 > 0) n2 = n2 * (radius / std::sqrt(len2));

      std::vector<typename K::Point_3> pts;
      const int num_pts = 16;
      for (int i = 0; i < num_pts; ++i) {
        double phi = 2.0 * M_PI * i / num_pts;
        typename K::Vector_3 n = std::cos(phi) * n1 + std::sin(phi) * n2;
        pts.push_back(p0 + n);
        pts.push_back(p1 + n);
      }

      CGAL::Surface_mesh<typename K::Point_3> segment_cylinder;
      CGAL::convex_hull_3(pts.begin(), pts.end(), segment_cylinder);

      if (tube_mesh.is_empty()) {
        tube_mesh = segment_cylinder;
      } else {
        CGAL::Polygon_mesh_processing::corefine_and_compute_union(
            tube_mesh, segment_cylinder, tube_mesh);
      }
    }
  }
}

template <typename Mesh, typename SideOf>
static bool is_in_roi(typename Mesh::Face_index f, const Mesh& mesh,
                      const SideOf& side_of) {
  if (f == Mesh::null_face()) return false;
  typename Mesh::Point center(0, 0, 0);
  int count = 0;
  for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
    center = center + (mesh.point(v) - CGAL::ORIGIN);
    count++;
  }
  center = CGAL::ORIGIN + (center - CGAL::ORIGIN) / count;
  return side_of(center) != CGAL::ON_UNBOUNDED_SIDE;
}

static GeometryId Smooth(Assets& assets, Shape& shape,
                         std::vector<Shape>& polylines, double radius,
                         double angle_threshold, double resolution,
                         bool skip_fairing, bool skip_refine,
                         int fairing_continuity) {
  if (!shape.HasGeometryId()) return assets.UndefinedId();

  Geometry target_geom =
      assets.GetGeometry(shape.GeometryId()).Transform(shape.GetTf());

  // Using EK for corefinement to maintain precision.
  CGAL::Surface_mesh<EK::Point_3> ek_mesh;
  target_geom.EncodeSurfaceMesh<EK>(ek_mesh);
  repair_degeneracies<EK>(ek_mesh);

  if (ek_mesh.is_empty()) return assets.TextId(target_geom);

  std::cout << "Smooth: Creating tube mesh (EK)..." << std::endl;
  CGAL::Surface_mesh<EK::Point_3> tool_mesh;
  create_tube_around_polylines<EK>(polylines, radius, assets, tool_mesh);
  repair_degeneracies<EK>(tool_mesh);

  if (tool_mesh.is_empty()) {
    std::cout << "Smooth: Tube mesh is empty, aborting." << std::endl;
    return assets.TextId(target_geom);
  }

  std::cout << "Smooth: Corefining (EK)..." << std::endl;
  CGAL::Polygon_mesh_processing::corefine(ek_mesh, tool_mesh);
  CGAL::Polygon_mesh_processing::triangulate_faces(ek_mesh);
  repair_degeneracies<EK>(ek_mesh);
  std::cout << "Smooth: Corefine done." << std::endl;

  // Convert to IK for Remeshing and Fairing
  std::cout << "Smooth: Converting to IK for Remeshing/Fairing..." << std::endl;
  CGAL::Surface_mesh<IK::Point_3> mesh;
  CGAL::copy_face_graph(ek_mesh, mesh);
  repair_degeneracies<IK>(mesh);

  CGAL::Surface_mesh<IK::Point_3> ik_tool_mesh;
  CGAL::copy_face_graph(tool_mesh, ik_tool_mesh);
  repair_degeneracies<IK>(ik_tool_mesh);

  CGAL::Side_of_triangle_mesh<CGAL::Surface_mesh<IK::Point_3>, IK> side_of(
      ik_tool_mesh);

  // Setup property map for patch selection.
  auto selected =
      mesh.add_property_map<CGAL::Surface_mesh<IK::Point_3>::Face_index, bool>(
              "f:selected", false)
          .first;

  std::vector<CGAL::Surface_mesh<IK::Point_3>::Face_index> roi_faces;
  for (auto f : mesh.faces()) {
    if (is_in_roi(f, mesh, side_of)) {
      selected[f] = true;
      roi_faces.push_back(f);
    }
  }

  if (roi_faces.empty()) {
    std::cout << "Smooth: ROI is empty, aborting." << std::endl;
    return assets.TextId(target_geom);
  }

  // Calculate target edge length for remeshing based on radius and threshold.
  // angle_threshold is in tau (0..1).
  double L_min = radius * (angle_threshold * 2.0 * M_PI);
  if (L_min <= 0) L_min = radius * 0.1;

  // Adjust L_min based on resolution (higher resolution = smaller L_min).
  if (resolution > 0) {
    L_min /= (resolution / 4.0);
  }

  // Identify ALL edges in the current ROI.
  std::set<CGAL::Surface_mesh<IK::Point_3>::Edge_index> all_roi_edges;
  for (auto f : roi_faces) {
    for (auto h : mesh.halfedges_around_face(mesh.halfedge(f))) {
      all_roi_edges.insert(mesh.edge(h));
    }
  }
  std::vector<CGAL::Surface_mesh<IK::Point_3>::Edge_index> all_roi_edges_vec(
      all_roi_edges.begin(), all_roi_edges.end());

  // UNCONDITIONAL PRE-SPLIT of ROI edges to provide density and satisfy
  // preconditions. We use the 'selected' map so that new faces created by
  // splitting inherit the ROI status.
  std::cout << "Smooth: Pre-splitting all ROI edges (IK) L_min=" << L_min
            << "..." << std::endl;
  CGAL::Polygon_mesh_processing::split_long_edges(
      all_roi_edges_vec, L_min, mesh,
      CGAL::parameters::face_patch_map(selected));

  // RE-COLLECT ROI faces because split_long_edges created many new ones and
  // updated 'selected'.
  roi_faces.clear();
  for (auto f : mesh.faces()) {
    if (selected[f]) {
      roi_faces.push_back(f);
    }
  }

  if (!skip_refine) {
    std::cout << "Smooth: Isotropic remeshing (IK) patch size="
              << roi_faces.size() << " L_min=" << L_min << "..." << std::endl;
    // Remeshing with protection ENABLED.
    // isotropic_remeshing automatically detects and protects the patch boundary
    // when given a face range and protect_constraints(true).
    CGAL::Polygon_mesh_processing::isotropic_remeshing(
        roi_faces, L_min, mesh,
        CGAL::parameters::face_patch_map(selected)
            .protect_constraints(true)
            .relax_constraints(true)
            .number_of_iterations(10));
    repair_degeneracies<IK>(mesh);
    std::cout << "Smooth: Remeshing done." << std::endl;
  }

  // Identify vertices to fair.
  std::vector<CGAL::Surface_mesh<IK::Point_3>::Vertex_index> fairing_patch;
  for (auto v : mesh.vertices()) {
    if (mesh.is_removed(v)) continue;
    bool has_in = false;
    bool has_out = false;

    if (CGAL::is_border(v, mesh)) {
      has_out = true;
    }

    for (auto f : CGAL::faces_around_target(mesh.halfedge(v), mesh)) {
      if (f == CGAL::Surface_mesh<IK::Point_3>::null_face()) continue;
      if (selected[f]) {
        has_in = true;
      } else {
        has_out = true;
      }
    }

    // A vertex is fairable if it's inside the ROI and not on the boundary of
    // the patch.
    if (has_in && !has_out) {
      fairing_patch.push_back(v);
    }
  }

  if (!skip_fairing && !fairing_patch.empty()) {
    std::cout << "Smooth: Fairing " << fairing_patch.size()
              << " vertices with continuity " << fairing_continuity << "..."
              << std::endl;
    CGAL::Polygon_mesh_processing::fair(
        mesh, fairing_patch,
        CGAL::parameters::fairing_continuity(fairing_continuity));
  }
  std::cout << "Smooth: Fairing done." << std::endl;

  if (!CGAL::is_valid_polygon_mesh(mesh)) {
    std::cout << "Smooth: Result mesh is INVALID!" << std::endl;
  }
  if (!CGAL::is_closed(mesh)) {
    std::cout << "Smooth: Result mesh is NOT closed!" << std::endl;
  }

  Geometry output;
  output.DecodeSurfaceMesh<IK>(mesh);
  return assets.TextId(output.Transform(shape.GetTf().inverse()));
}

}  // namespace geometry
