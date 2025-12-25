#pragma once

#include <CGAL/Cartesian_converter.h>  // Include for point conversion
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>  // Include IK
#include <CGAL/Surface_mesh.h>
#include <CGAL/alpha_wrap_3.h>  // Include the free function header

#include "assets.h"
#include "geometry.h"
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
                        double offset = 0.0) {
  // Collect all geometry from the shape and its sub-shapes
  Geometry combined_geometry;

  shape.Walk([&](Shape& sub_shape) {
    if (!sub_shape.HasGeometryId()) {
      return true;
    }
    // Get transformed geometry
    Geometry geom =
        assets.GetGeometry(sub_shape.GeometryId()).Transform(sub_shape.GetTf());

    // Append vertices and update triangle indices
    size_t current_vertex_offset = combined_geometry.vertices_.size();
    for (const auto& v_cgal : geom.vertices_) {
      combined_geometry.AddVertex(v_cgal);
    }
    for (const auto& triangle : geom.triangles_) {
      std::vector<size_t> translated_triangle;
      translated_triangle.reserve(triangle.size());
      for (size_t index : triangle) {
        translated_triangle.push_back(index + current_vertex_offset);
      }
      combined_geometry.triangles_.push_back(translated_triangle);
    }
    // Note: alpha_wrap_3 operates on points and faces (triangles), segments are
    // not directly used here.
    return true;
  });

  if (combined_geometry.vertices_.empty() ||
      combined_geometry.triangles_.empty()) {
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
  // The combined_geometry.triangles_ already is in the correct format for
  // FaceRange (std::vector<std::vector<size_t>>)

  OutputMesh alpha_wrapped_mesh;

  // Call CGAL::alpha_wrap_3
  CGAL::alpha_wrap_3(input_points,
                     combined_geometry.triangles_,  // This is the FaceRange
                     alpha, offset, alpha_wrapped_mesh
                     // No named parameters for now, can add if needed
  );

  // Convert the output Surface_mesh back to Geometry, converting points back to
  // EK_Kernel
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