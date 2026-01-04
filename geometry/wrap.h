#pragma once

#include <CGAL/Cartesian_converter.h>  // Include for point conversion
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>  // Include IK
#include <CGAL/Surface_mesh.h>
#include <CGAL/alpha_wrap_3.h>  // Include the free function header

#include "assets.h"
#include "geometry.h"
#include "random_util.h"
#include "shape.h"

// Define the kernel type for alpha_wrap_3 (EPICK)
typedef CGAL::Exact_predicates_inexact_constructions_kernel IK_Kernel;
typedef IK_Kernel::Point_3 IK_Point;
typedef CGAL::Surface_mesh<IK_Point>
    OutputMesh;  // alpha_wrap_3 can output to Surface_mesh

// Use EK (Exact Kernel) for Geometry's internal points
typedef CGAL::Exact_predicates_exact_constructions_kernel EK_Kernel;
typedef EK_Kernel::Point_3 EK_Point;

static GeometryId Wrap3(Assets& assets, Shape& shape, double alpha = 1.0,
                        double offset = 1.0) {
  // Collect all geometry from the shape and its sub-shapes
  Geometry combined_geometry;

  shape.Walk([&](Shape& sub_shape) {
    if (!sub_shape.HasGeometryId()) {
      return true;
    }
    // Get transformed geometry
    Geometry geom =
        assets.GetGeometry(sub_shape.GeometryId()).Transform(sub_shape.GetTf());

    // Use a temporary mesh to get triangulated faces and existing triangles
    CGAL::Surface_mesh<EK_Point> temp_mesh;
    geom.EncodeSurfaceMesh<EK_Kernel>(temp_mesh);

    // Create a mapping from temp_mesh vertex indices to combined_geometry
    // merged vertex indices
    std::map<CGAL::Surface_mesh<EK_Point>::Vertex_index, size_t> v_map;
    for (auto v : temp_mesh.vertices()) {
      v_map[v] = combined_geometry.AddVertex(temp_mesh.point(v), true);
    }

    // Add all faces from temp_mesh as triangles to combined_geometry
    for (auto f : temp_mesh.faces()) {
      std::vector<size_t> tri;
      for (auto v : temp_mesh.vertices_around_face(temp_mesh.halfedge(f))) {
        tri.push_back(v_map[v]);
      }
      combined_geometry.triangles_.push_back(tri);
    }

    // Add segments as non-degenerate triangles [s, t, t + iota]
    for (const auto& [source, target] : geom.segments_) {
      size_t s_idx = combined_geometry.AddVertex(geom.vertices_[source], true);
      size_t t_idx = combined_geometry.AddVertex(geom.vertices_[target], true);
      const auto& t_pt = geom.vertices_[target];
      size_t ti_idx = combined_geometry.AddVertex(
          EK_Point(t_pt.x() + 1e-9, t_pt.y() + 1e-9, t_pt.z() + 1e-9), true);
      combined_geometry.triangles_.push_back({s_idx, t_idx, ti_idx});
    }

    // Add points as non-degenerate triangles [p, p + iota1, p + iota2]
    for (const auto& point_idx : geom.points_) {
      const auto& p_pt = geom.vertices_[point_idx];
      size_t p_idx = combined_geometry.AddVertex(p_pt, true);
      size_t pi1_idx = combined_geometry.AddVertex(
          EK_Point(p_pt.x() + 1e-9, p_pt.y(), p_pt.z()), true);
      size_t pi2_idx = combined_geometry.AddVertex(
          EK_Point(p_pt.x(), p_pt.y() + 1e-9, p_pt.z()), true);
      combined_geometry.triangles_.push_back({p_idx, pi1_idx, pi2_idx});
    }

    return true;
  });

  if (combined_geometry.vertices_.empty()) {
    // If there's no geometry, return an empty ID
    return assets.UndefinedId();
  }

  // Prepare inputs for CGAL::alpha_wrap_3, converting points to IK_Kernel
  CGAL::Cartesian_converter<EK_Kernel, IK_Kernel> ek_to_ik_converter;
  std::vector<IK_Point> input_points;
  input_points.reserve(combined_geometry.vertices_.size());
  for (const auto& v_ek : combined_geometry.vertices_) {
    input_points.push_back(ek_to_ik_converter(v_ek));
  }

  OutputMesh alpha_wrapped_mesh;
  make_deterministic();

  if (combined_geometry.triangles_.empty()) {
    CGAL::alpha_wrap_3(input_points, alpha, offset, alpha_wrapped_mesh);

  } else {
    CGAL::alpha_wrap_3(input_points, combined_geometry.triangles_, alpha,
                       offset, alpha_wrapped_mesh);
  }
  CGAL::Cartesian_converter<IK_Kernel, EK_Kernel> ik_to_ek_converter;
  Geometry wrapped_geometry;
  wrapped_geometry.vertices_.reserve(alpha_wrapped_mesh.number_of_vertices());
  for (OutputMesh::Vertex_index vi : alpha_wrapped_mesh.vertices()) {
    wrapped_geometry.AddVertex(
        ik_to_ek_converter(alpha_wrapped_mesh.point(vi)));
  }
  wrapped_geometry.triangles_.reserve(alpha_wrapped_mesh.number_of_faces());
  for (OutputMesh::Face_index fi : alpha_wrapped_mesh.faces()) {
    std::vector<size_t> face_indices;
    face_indices.reserve(3);
    // Iterate around the face to get vertex indices
    for (OutputMesh::Halfedge_index hi : halfedges_around_face(
             alpha_wrapped_mesh.halfedge(fi), alpha_wrapped_mesh)) {
      face_indices.push_back(alpha_wrapped_mesh.source(hi).idx());
    }
    wrapped_geometry.triangles_.push_back(face_indices);
  }

  return assets.TextId(wrapped_geometry);
}
