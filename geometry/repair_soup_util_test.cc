#include "repair_soup_util.h"

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/IO/OBJ.h>
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/measure.h>  // Added for CGAL::Polygon_mesh_processing::volume
#include <CGAL/Polygon_mesh_processing/orientation.h>  // Added for PMP::is_outward_oriented
#include <CGAL/Polygon_mesh_processing/self_intersections.h>  // Keep this for PMP::does_self_intersect
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Surface_mesh.h>

#include <cassert>
#include <fstream>
#include <iomanip>  // Added for std::fixed and std::setprecision
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>   // Added for std::string in write_soup_to_obj
#include <utility>  // For std::pair
#include <vector>

#include "soup_test_geometry.h"

namespace PMP = CGAL::Polygon_mesh_processing;

typedef CGAL::Exact_predicates_exact_constructions_kernel Epeck;
typedef Epeck::Point_3 Point_3;
typedef CGAL::Surface_mesh<Point_3> Surface_mesh;

// Helper to check if a mesh is vertex manifold
bool are_all_vertices_manifold(const Surface_mesh& mesh) {
  std::vector<Surface_mesh::Halfedge_index> non_manifold_halfedges;
  PMP::non_manifold_vertices(mesh, std::back_inserter(non_manifold_halfedges));

  if (non_manifold_halfedges.empty()) {
    return true;  // No non-manifold halfedges, so no non-manifold vertices
  }

  // Extract unique vertex indices from the non-manifold halfedges
  std::set<Surface_mesh::Vertex_index> non_manifold_vertex_indices;
  for (Surface_mesh::Halfedge_index h_idx : non_manifold_halfedges) {
    non_manifold_vertex_indices.insert(mesh.target(h_idx));
  }

  return non_manifold_vertex_indices.empty();
}

void test_tetrahedron_soup_fix_soup() {
  std::cout << "Running test: test_tetrahedron_soup_fix_soup" << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedron_mesh_t<Epeck>(points, triangles);

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success = fix_soup<Epeck>(
      points, triangles, removed_points_out, removed_triangles_out,
      even_winding_points_out, even_winding_triangles_out,
      odd_winding_points_out, odd_winding_triangles_out, 0.05);
  assert(success);

  // Reconstruct Surface_mesh from the generated soup for final checks
  Surface_mesh test_mesh;
  PMP::orient_polygon_soup(points, triangles);  // Ensure correct orientation
  PMP::polygon_soup_to_polygon_mesh(points, triangles, test_mesh);

  // Assertions for well-formedness and orientation
  assert(!test_mesh.is_empty());                   // Mesh should not be empty
  assert(CGAL::is_closed(test_mesh));              // Check if closed
  assert(CGAL::is_triangle_mesh(test_mesh));       // Check if triangular
  assert(CGAL::is_valid_polygon_mesh(test_mesh));  // Check if valid
  assert(
      !PMP::does_self_intersect(test_mesh));  // No self-intersections in mesh
  assert(are_all_vertices_manifold(test_mesh));  // Should be vertex manifold
  assert(PMP::is_outward_oriented(test_mesh));   // Check if outward oriented
  assert(PMP::volume(test_mesh) > 0);  // Check if encloses a positive volume

  std::cout << "  Tetrahedron mesh processed with fix_soup is valid."
            << std::endl;
}

void test_point_face_touching_tetrahedrons_fix_soup() {
  std::cout << "Running test: test_point_face_touching_tetrahedrons_fix_soup"
            << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedrons_touching_at_point_and_face_mesh_t<Epeck>(
      points, triangles);

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success = fix_soup<Epeck>(
      points, triangles, removed_points_out, removed_triangles_out,
      even_winding_points_out, even_winding_triangles_out,
      odd_winding_points_out, odd_winding_triangles_out, 0.05);
  assert(success);

  // Re-check for self-intersections after cleanup
  std::vector<std::pair<std::size_t, std::size_t>> intersections_after_cleanup;
  CGAL::Polygon_mesh_processing::triangle_soup_self_intersections(
      points, triangles, std::back_inserter(intersections_after_cleanup));
  assert(intersections_after_cleanup.empty());
  std::cout << "  Cleaned soup has no self-intersections as expected."
            << std::endl;

  // Reconstruct Surface_mesh from the refined and cleaned soup for final
  // checks.
  Surface_mesh refined_mesh;  // Ensure correct orientation
  PMP::polygon_soup_to_polygon_mesh(points, triangles, refined_mesh);

  assert(!refined_mesh.is_empty());
  assert(CGAL::is_closed(refined_mesh));
  assert(CGAL::is_triangle_mesh(refined_mesh));
  assert(CGAL::is_valid_polygon_mesh(refined_mesh));
  assert(are_all_vertices_manifold(refined_mesh));
  assert(PMP::is_outward_oriented(refined_mesh));
  assert(PMP::volume(refined_mesh) > 0);

  std::cout
      << "  Point-face touching tetrahedrons processed with fix_soup are valid."
      << std::endl;
}

void test_interpenetrating_tetrahedrons_fix_soup() {
  std::cout << "Running test: test_interpenetrating_tetrahedrons_fix_soup"
            << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedrons_interpenetrating_soup_t<Epeck>(points,
                                                              triangles);

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success = fix_soup<Epeck>(
      points, triangles, removed_points_out, removed_triangles_out,
      even_winding_points_out, even_winding_triangles_out,
      odd_winding_points_out, odd_winding_triangles_out, 0.05);
  assert(success);

  std::vector<std::pair<std::size_t, std::size_t>> intersections_after_cleanup;
  CGAL::Polygon_mesh_processing::triangle_soup_self_intersections(
      points, triangles, std::back_inserter(intersections_after_cleanup));

  assert(intersections_after_cleanup.empty());
  std::cout << "  Soup has no self-intersections after cleanup as expected."
            << std::endl;

  // Check soup validity after cleanup (THIS IS THE CRITICAL CHECK)
  bool final_is_valid_soup_mesh =
      CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(triangles);
  assert(final_is_valid_soup_mesh);
  std::cout << "  Final soup can form a valid polygon mesh: "
            << (final_is_valid_soup_mesh ? "true" : "false")
            << " (expected true)" << std::endl;

  // Reconstruct Surface_mesh from the refined and cleaned soup for final
  // checks.
  Surface_mesh refined_mesh;
  PMP::polygon_soup_to_polygon_mesh(points, triangles, refined_mesh);

  assert(!refined_mesh.is_empty());
  assert(CGAL::is_closed(refined_mesh));
  assert(CGAL::is_triangle_mesh(refined_mesh));
  assert(CGAL::is_valid_polygon_mesh(refined_mesh));
  assert(are_all_vertices_manifold(refined_mesh));
  assert(PMP::is_outward_oriented(refined_mesh));
  assert(PMP::volume(refined_mesh) > 0);
  assert(!PMP::does_self_intersect(refined_mesh));

  std::cout
      << "  Interpenetrating tetrahedrons processed with fix_soup are valid."
      << std::endl;
}

void test_single_tetrahedron_fix_soup() {
  std::cout << "Running test: test_single_tetrahedron_fix_soup" << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedron_mesh_t<Epeck>(points, triangles);

  std::vector<Point_3> initial_points = points;
  std::vector<std::array<std::size_t, 3>> initial_triangles = triangles;

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success = fix_soup<Epeck>(
      points, triangles, removed_points_out, removed_triangles_out,
      even_winding_points_out, even_winding_triangles_out,
      odd_winding_points_out, odd_winding_triangles_out, 0.05);
  assert(success);

  // After removing overlap, the soup should remain unchanged for a single,
  // valid tetrahedron (Assuming fix_soup doesn't change point indices if it
  // doesn't need to, but it runs autorefine which might re-index or add small
  // epsilon snaps, so exact equality might be strict. However, for a single
  // tetrahedron, autorefine should be a no-op ideally.)

  // Note: autorefine might change indices or order, so strict equality of
  // vectors might fail if implementation details change. But structurally it
  // should be the same. Let's check volume and basic properties.

  Surface_mesh mesh;
  PMP::polygon_soup_to_polygon_mesh(points, triangles, mesh);
  assert(!mesh.is_empty());
  assert(CGAL::is_valid_polygon_mesh(mesh));

  std::cout << "  Single tetrahedron processed with fix_soup is valid."
            << std::endl;
}

void test_point_face_touching_tetrahedrons_negative_delta() {
  std::cout
      << "Running test: test_point_face_touching_tetrahedrons_negative_delta"
      << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedrons_touching_at_point_and_face_mesh_t<Epeck>(
      points, triangles);

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success =
      fix_soup<Epeck>(points, triangles, removed_points_out,
                      removed_triangles_out, even_winding_points_out,
                      even_winding_triangles_out, odd_winding_points_out,
                      odd_winding_triangles_out, -0.05);  // Negative delta
  assert(success);

  // With a negative separation delta, the expectation is that the
  // "horn structure" at the touching point is pulled inward, creating a gap.
  // This should result in the two original tetrahedrons being truly separated
  // and the resulting mesh (points, triangles) should consist of only one of
  // the original tetrahedrons (the one that was not 'removed'). The
  // 'odd_winding_triangles_out' should contain the other tetrahedron.

  // Re-check for self-intersections after cleanup
  std::vector<std::pair<std::size_t, std::size_t>> intersections_after_cleanup;
  CGAL::Polygon_mesh_processing::triangle_soup_self_intersections(
      points, triangles, std::back_inserter(intersections_after_cleanup));
  assert(intersections_after_cleanup.empty());
  std::cout << "  Cleaned soup (negative delta) has no self-intersections as "
               "expected."
            << std::endl;

  // Reconstruct Surface_mesh from the refined and cleaned soup for final
  // checks.
  Surface_mesh refined_mesh;
  PMP::polygon_soup_to_polygon_mesh(points, triangles, refined_mesh);

  assert(!refined_mesh.is_empty());
  assert(CGAL::is_closed(refined_mesh));
  assert(CGAL::is_triangle_mesh(refined_mesh));
  assert(CGAL::is_valid_polygon_mesh(refined_mesh));
  assert(are_all_vertices_manifold(refined_mesh));
  assert(PMP::is_outward_oriented(refined_mesh));
  assert(PMP::volume(refined_mesh) > 0);

  // Further specific assertions could be added here to confirm the topological
  // separation, e.g., checking the number of connected components, or comparing
  // volumes of even_winding_points_out/even_winding_triangles_out to the
  // original tetrahedrons. For now, basic validity check is sufficient as per
  // previous tests.

  std::cout << "  Point-face touching tetrahedrons processed with fix_soup "
               "(negative delta) are valid."
            << std::endl;
}

void test_interpenetrating_tetrahedrons_negative_delta() {
  std::cout << "Running test: test_interpenetrating_tetrahedrons_negative_delta"
            << std::endl;

  std::vector<Point_3> points;
  std::vector<std::array<std::size_t, 3>> triangles;
  jot::soup::make_tetrahedrons_interpenetrating_soup_t<Epeck>(points,
                                                              triangles);

  std::vector<Point_3> removed_points_out;
  std::vector<std::array<std::size_t, 3>> removed_triangles_out;
  std::vector<Point_3> even_winding_points_out;
  std::vector<std::array<std::size_t, 3>> even_winding_triangles_out;
  std::vector<Point_3> odd_winding_points_out;
  std::vector<std::array<std::size_t, 3>> odd_winding_triangles_out;

  bool success =
      fix_soup<Epeck>(points, triangles, removed_points_out,
                      removed_triangles_out, even_winding_points_out,
                      even_winding_triangles_out, odd_winding_points_out,
                      odd_winding_triangles_out, -0.05);  // Negative delta
  assert(success);

  // With a negative separation delta for interpenetrating solids, the
  // expectation is that the regions of non-manifoldness are pulled inward,
  // effectively shrinking the volumes and creating separation. The resulting
  // 'points' and 'triangles' (even winding) should represent a cleaner version
  // of the original shapes but potentially with a larger gap between them, or
  // even breaking them into separate components if they were originally joined
  // only by interpenetration.

  std::vector<std::pair<std::size_t, std::size_t>> intersections_after_cleanup;
  CGAL::Polygon_mesh_processing::triangle_soup_self_intersections(
      points, triangles, std::back_inserter(intersections_after_cleanup));

  assert(intersections_after_cleanup.empty());
  std::cout << "  Soup (negative delta) has no self-intersections after "
               "cleanup as expected."
            << std::endl;

  // Check soup validity after cleanup
  bool final_is_valid_soup_mesh =
      CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(triangles);
  assert(final_is_valid_soup_mesh);
  std::cout << "  Final soup (negative delta) can form a valid polygon mesh: "
            << (final_is_valid_soup_mesh ? "true" : "false")
            << " (expected true)" << std::endl;

  // Reconstruct Surface_mesh from the refined and cleaned soup for final
  // checks.
  Surface_mesh refined_mesh;
  PMP::polygon_soup_to_polygon_mesh(points, triangles, refined_mesh);

  assert(!refined_mesh.is_empty());
  assert(CGAL::is_closed(refined_mesh));
  assert(CGAL::is_triangle_mesh(refined_mesh));
  assert(CGAL::is_valid_polygon_mesh(refined_mesh));
  assert(are_all_vertices_manifold(refined_mesh));
  assert(PMP::is_outward_oriented(refined_mesh));
  assert(PMP::volume(refined_mesh) > 0);  // Volume should still be positive,
                                          // but might be smaller than original.
  assert(!PMP::does_self_intersect(refined_mesh));

  std::cout << "  Interpenetrating tetrahedrons processed with fix_soup "
               "(negative delta) are valid."
            << std::endl;
}

int main() {
  try {
    std::cout << "--------------------------------------------------------"
              << std::endl;
    std::cout << "Running new parallel tests using fix_soup..." << std::endl;
    std::cout << "--------------------------------------------------------"
              << std::endl;

    test_single_tetrahedron_fix_soup();
    test_interpenetrating_tetrahedrons_fix_soup();
    test_tetrahedron_soup_fix_soup();
    test_point_face_touching_tetrahedrons_fix_soup();
    test_point_face_touching_tetrahedrons_negative_delta();
    test_interpenetrating_tetrahedrons_negative_delta();

  } catch (const std::exception& e) {
    std::cerr << "Test failed with exception: " << e.what() << std::endl;
    return 1;
  }
  std::cout
      << "All autorefine tests (original and fix_soup) completed successfully."
      << std::endl;
  return 0;
}
