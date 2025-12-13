#pragma once

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Line_3.h>
#include <CGAL/Plane_3.h>
#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/merge_border_vertices.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <CGAL/Polygon_mesh_processing/repair_self_intersections.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Segment_3.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Triangle_3.h>
#include <CGAL/intersections.h>

#include <variant>

#include "unit_util.h"

// Creates a triangular prism mesh oriented along the segment from p1 to p2.
// The prism is extended by 'epsilon' at both ends.
// The cross-section is an equilateral triangle with a side length related to
// 'radius'.
template <typename K>
static void make_segment_envelope(
    const typename K::Point_3& p1, const typename K::Point_3& p2,
    typename K::FT epsilon, typename K::FT radius,
    CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef typename K::Point_3 Point_3;
  typedef typename K::Vector_3 Vector_3;
  typedef typename CGAL::Surface_mesh<Point_3>::Vertex_index Vertex_index;

  // 1. Define and extend the prism's axis
  Vector_3 axis_vec = p2 - p1;
  Vector_3 norm_axis_vec =
      axis_vec / std::sqrt(CGAL::to_double(axis_vec.squared_length()));
  Point_3 p_start = p1 - (norm_axis_vec * epsilon);
  Point_3 p_end = p2 + (norm_axis_vec * epsilon);
  Vector_3 extended_axis_vec = p_end - p_start;

  // 2. Construct a perpendicular frame
  Vector_3 u;
  // Try to find a non-collinear vector to cross with
  // If extended_axis_vec is not parallel to Z-axis, cross with Z-axis
  if (CGAL::to_double(extended_axis_vec.x()) != 0 ||
      CGAL::to_double(extended_axis_vec.y()) != 0) {
    u = CGAL::cross_product(extended_axis_vec, Vector_3(0, 0, 1));
  } else {
    // If extended_axis_vec is parallel to Z-axis, cross with X-axis
    u = CGAL::cross_product(extended_axis_vec, Vector_3(1, 0, 0));
  }
  u = u / std::sqrt(CGAL::to_double(u.squared_length()));  // Normalize u
  Vector_3 v = CGAL::cross_product(extended_axis_vec, u);
  v = v / std::sqrt(CGAL::to_double(v.squared_length()));  // Normalize v

  // 3. Create the base triangle vertices around p_start
  Point_3 v1 = p_start + (u * radius);
  Point_3 v2 = p_start - (u * (radius / 2)) + (v * (radius * std::sqrt(3) / 2));
  Point_3 v3 = p_start - (u * (radius / 2)) - (v * (radius * std::sqrt(3) / 2));

  // 4. Extrude the triangle to create top vertices around p_end
  Point_3 v4 = v1 + extended_axis_vec;
  Point_3 v5 = v2 + extended_axis_vec;
  Point_3 v6 = v3 + extended_axis_vec;

  // 5. Add vertices to the mesh
  Vertex_index V1 = mesh.add_vertex(v1);
  Vertex_index V2 = mesh.add_vertex(v2);
  Vertex_index V3 = mesh.add_vertex(v3);
  Vertex_index V4 = mesh.add_vertex(v4);
  Vertex_index V5 = mesh.add_vertex(v5);
  Vertex_index V6 = mesh.add_vertex(v6);

  // 6. Build the prism faces (2 caps, 3 sides * 2 triangles)
  // Bottom cap
  mesh.add_face(V1, V3, V2);
  // Top cap
  mesh.add_face(V4, V5, V6);
  // Side 1
  mesh.add_face(V1, V2, V5);
  mesh.add_face(V1, V5, V4);
  // Side 2
  mesh.add_face(V2, V3, V6);
  mesh.add_face(V2, V6, V5);
  // Side 3
  mesh.add_face(V3, V1, V4);
  mesh.add_face(V3, V4, V6);
}

// Creates a small tetrahedron mesh around a point 'p' with a given 'radius'.
template <typename K>
static void make_point_envelope(const typename K::Point_3& p,
                                typename K::FT radius,
                                CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef typename K::Point_3 Point_3;
  typedef typename K::Vector_3 Vector_3;
  typedef typename CGAL::Surface_mesh<Point_3>::Vertex_index Vertex_index;

  // Vertices of the tetrahedron
  Vector_3 v_vecs[4] = {Vector_3(1, 1, 1), Vector_3(1, -1, -1),
                        Vector_3(-1, 1, -1), Vector_3(-1, -1, 1)};

  Vertex_index V[4];
  for (int i = 0; i < 4; ++i) {
    V[i] = mesh.add_vertex(p + (v_vecs[i] * radius));
  }

  // Faces of the tetrahedron (4 triangles)
  mesh.add_face(V[0], V[1], V[2]);
  mesh.add_face(V[0], V[3], V[1]);
  mesh.add_face(V[0], V[2], V[3]);
  mesh.add_face(V[1], V[3], V[2]);
  assert(CGAL::Polygon_mesh_processing::volume(mesh) > 0);
  assert(CGAL::is_closed(mesh));
  assert(CGAL::is_triangle_mesh(mesh));
  assert(CGAL::is_valid_polygon_mesh(mesh));
}

template <typename K>
static size_t get_self_intersections(
    const CGAL::Surface_mesh<typename K::Point_3>& mesh,
    std::vector<
        std::pair<typename CGAL::Surface_mesh<typename K::Point_3>::face_index,
                  typename CGAL::Surface_mesh<CGAL::Point_3<K>>::face_index>>&
        intersections) {
  CGAL::Polygon_mesh_processing::self_intersections(
      mesh, std::back_inserter(intersections));
  return intersections.size();
}

template <typename K>
static void make_triangle_envelope(
    const typename K::Point_3& base_p0_in,
    const typename K::Point_3& base_p1_in,
    const typename K::Point_3& base_p2_in,
    CGAL::Surface_mesh<typename K::Point_3>& mesh) {
  typedef typename CGAL::Surface_mesh<typename K::Point_3>::Vertex_index
      Vertex_index;
  typedef typename K::Vector_3 Vector_3;
  typedef typename K::Point_3 Point_3;
  typedef typename K::Line_3 Line_3;

  typename K::FT iota = 0.1;  // Offset amount

  // Original triangle points
  Point_3 p0_orig = base_p0_in;
  Point_3 p1_orig = base_p1_in;
  Point_3 p2_orig = base_p2_in;

  // 1. Calculate edge vectors
  Vector_3 e01_vec = p1_orig - p0_orig;
  Vector_3 e12_vec = p2_orig - p1_orig;
  Vector_3 e20_vec = p0_orig - p2_orig;

  // 2. Normalize edge vectors
  Vector_3 ne01 =
      e01_vec /
      static_cast<double>(std::sqrt(CGAL::to_double(e01_vec.squared_length())));
  Vector_3 ne12 =
      e12_vec /
      static_cast<double>(std::sqrt(CGAL::to_double(e12_vec.squared_length())));
  Vector_3 ne20 =
      e20_vec /
      static_cast<double>(std::sqrt(CGAL::to_double(e20_vec.squared_length())));

  // 3. Calculate triangle's plane normal
  Vector_3 plane_normal_unnorm = CGAL::cross_product(e01_vec, e12_vec);
  Vector_3 plane_normal =
      plane_normal_unnorm / static_cast<double>(std::sqrt(CGAL::to_double(
                                plane_normal_unnorm.squared_length())));

  // 4. Calculate inward-pointing edge normals (in the plane of the triangle)
  //    This ensures offsetting "expands" the triangle.
  Vector_3 en01 = CGAL::cross_product(plane_normal, ne01) * iota;
  Vector_3 en12 = CGAL::cross_product(plane_normal, ne12) * iota;
  Vector_3 en20 = CGAL::cross_product(plane_normal, ne20) * iota;

  // 5. Create offset lines
  Line_3 l01(p0_orig + en01,
             ne01);  // Line parallel to p0_orig-p1_orig, offset inwards
  Line_3 l12(p1_orig + en12,
             ne12);  // Line parallel to p1_orig-p2_orig, offset inwards
  Line_3 l20(p2_orig + en20,
             ne20);  // Line parallel to p2_orig-p0_orig, offset inwards

  // 6. Find intersections of offset lines to get expanded triangle vertices
  Point_3 expanded_p0, expanded_p1, expanded_p2;

  // Intersection of l20 and l01 gives expanded_p0
  auto intersect_l20_l01 = CGAL::intersection(l20, l01);
  if (intersect_l20_l01) {
    if (const Point_3* p_ptr = std::get_if<Point_3>(&(*intersect_l20_l01))) {
      expanded_p0 = *p_ptr;
    } else {
      assert(false && "Intersection for p0 is not a Point_3!");
    }
  } else {
    // Handle error: lines are parallel or do not intersect
    // For a triangle, they should intersect. If not, triangle is degenerate or
    // iota is too large.
    assert(false && "Expanded lines for p0 did not intersect!");
  }

  // Intersection of l01 and l12 gives expanded_p1
  auto intersect_l01_l12 = CGAL::intersection(l01, l12);
  if (intersect_l01_l12) {
    if (const Point_3* p_ptr = std::get_if<Point_3>(&(*intersect_l01_l12))) {
      expanded_p1 = *p_ptr;
    } else {
      assert(false && "Intersection for p1 is not a Point_3!");
    }
  } else {
    assert(false && "Expanded lines for p1 did not intersect!");
  }

  // Intersection of l12 and l20 gives expanded_p2
  auto intersect_l12_l20 = CGAL::intersection(l12, l20);
  if (intersect_l12_l20) {
    if (const Point_3* p_ptr = std::get_if<Point_3>(&(*intersect_l12_l20))) {
      expanded_p2 = *p_ptr;
    } else {
      assert(false && "Intersection for p2 is not a Point_3!");
    }
  } else {
    assert(false && "Expanded lines for p2 did not intersect!");
  }

  // Now use these expanded points as the base for the prism
  typename K::Point_3 bottom_p0(expanded_p0.x(), expanded_p0.y(),
                                expanded_p0.z() - iota);
  typename K::Point_3 bottom_p1(expanded_p1.x(), expanded_p1.y(),
                                expanded_p1.z() - iota);
  typename K::Point_3 bottom_p2(expanded_p2.x(), expanded_p2.y(),
                                expanded_p2.z() - iota);

  typename K::Point_3 top_p0(expanded_p0.x(), expanded_p0.y(),
                             expanded_p0.z() + iota);
  typename K::Point_3 top_p1(expanded_p1.x(), expanded_p1.y(),
                             expanded_p1.z() + iota);
  typename K::Point_3 top_p2(expanded_p2.x(), expanded_p2.y(),
                             expanded_p2.z() + iota);

  // Bottom vertices
  Vertex_index b0 = mesh.add_vertex(bottom_p0);
  Vertex_index b1 = mesh.add_vertex(bottom_p1);
  Vertex_index b2 = mesh.add_vertex(bottom_p2);

  // Top vertices
  Vertex_index t0 = mesh.add_vertex(top_p0);
  Vertex_index t1 = mesh.add_vertex(top_p1);
  Vertex_index t2 = mesh.add_vertex(top_p2);

  // Bottom face (CW to point normal -Z, which is outward for bottom)
  assert(mesh.add_face(b0, b2, b1) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());

  // Top face (CCW to point normal +Z, which is outward for top)
  assert(mesh.add_face(t0, t1, t2) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());

  // Side faces (each quad as two triangles with consistent winding)
  // Side 1: quad (b0, b1, t1, t0) -> triangles (b0, b1, t1) and (b0, t1, t0)
  assert(mesh.add_face(b0, b1, t1) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());
  assert(mesh.add_face(b0, t1, t0) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());

  // Side 2: quad (b1, b2, t2, t1) -> triangles (b1, b2, t2) and (b1, t2, t1)
  assert(mesh.add_face(b1, b2, t2) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());
  assert(mesh.add_face(b1, t2, t1) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());

  // Side 3: quad (b2, b0, t0, t2) -> triangles (b2, b0, t0) and (b2, t0, t2)
  assert(mesh.add_face(b2, b0, t0) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());
  assert(mesh.add_face(b2, t0, t2) !=
         CGAL::Surface_mesh<typename K::Point_3>::null_face());
}
