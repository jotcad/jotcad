#pragma once

#include <CGAL/Polygon_mesh_processing/manifoldness.h>

#include "manifold_util.h"

enum ValidateStrategy {
  VALIDATE_IS_NOT_SELF_INTERSECTING = 0,
  VALIDATE_IS_CLOSED = 1,
  VALIDATE_IS_MANIFOLD = 2,
  VALIDATE_IS_NOT_DEGENERATE = 3,
};

template <typename Surface_mesh>
static bool validate(const Surface_mesh& mesh, std::vector<int> strategies) {
  bool valid = true;
  for (int strategy : strategies) {
    switch (strategy) {
      case VALIDATE_IS_NOT_SELF_INTERSECTING: {
        if (CGAL::Polygon_mesh_processing::does_self_intersect(mesh)) {
          std::cout << "validate: failed is not self intersecting check."
                    << std::endl;
          valid = false;
          break;
        }
        break;
      }
      case VALIDATE_IS_CLOSED: {
        if (!CGAL::is_closed(mesh)) {
          std::cout << "validate: failed is closed check." << std::endl;
          valid = false;
          break;
        }
        break;
      }
      case VALIDATE_IS_MANIFOLD: {
        if (CGAL::is_closed(mesh)) {
#ifdef JOT_MANIFOLD_ENABLE
          if (!validate_with_manifold(mesh)) {
            std::cout << "validate: failed validate_with_manifold test."
                      << std::endl;
            valid = false;
          }
#endif JOT_MANIFOLD_ENABLE
        }
        std::vector<typename Surface_mesh::Halfedge_index> non_manifold;
        CGAL::Polygon_mesh_processing::non_manifold_vertices(
            mesh, std::back_inserter(non_manifold));
        if (!non_manifold.empty()) {
          std::cout << "validate: failed is manifold check." << std::endl;
          valid = false;
          for (const auto& vertex : non_manifold) {
            std::cout << "validate: non-manifold vertex " << vertex
                      << std::endl;
          }
        }
        break;
      }
      case VALIDATE_IS_NOT_DEGENERATE: {
        std::vector<typename Surface_mesh::Edge_index> degenerate_edges;
        CGAL::Polygon_mesh_processing::degenerate_edges(
            mesh, std::back_inserter(degenerate_edges));
        std::vector<typename Surface_mesh::Face_index> degenerate_faces;
        CGAL::Polygon_mesh_processing::degenerate_faces(
            mesh, std::back_inserter(degenerate_faces));
        if (!degenerate_edges.empty()) {
          valid = false;
          std::cout << "validate: failed is not degenerate edges check."
                    << std::endl;
        }
        if (!degenerate_faces.empty()) {
          valid = false;
          std::cout << "validate: failed is not degenerate faces check."
                    << std::endl;
        }
        break;
      }
    }
  }
  if (!valid) {
    std::cout << "validate: failed" << std::endl;
  }
  return valid;
}

template <typename Surface_mesh>
static bool validate_mesh(const Surface_mesh& mesh, const std::string& label,
                          std::vector<std::string>* error_tokens = nullptr) {
  bool valid = true;
  if (!CGAL::is_triangle_mesh(mesh)) {
    std::cout << label << " ASSERTION FAILED: mesh is not a triangle mesh!"
              << std::endl;
    if (error_tokens) error_tokens->push_back("notTriangleMesh");
    valid = false;
  }
  if (!CGAL::is_valid_polygon_mesh(mesh)) {
    std::cout << label << " ASSERTION FAILED: mesh is not a valid polygon mesh!"
              << std::endl;
    if (error_tokens) error_tokens->push_back("notValidPolygonMesh");
    valid = false;
  }
  if (CGAL::Polygon_mesh_processing::does_self_intersect(mesh)) {
    std::cout << label << " ASSERTION FAILED: mesh has self-intersections!"
              << std::endl;
    if (error_tokens) error_tokens->push_back("selfIntersecting");
    valid = false;
  }
  std::vector<typename Surface_mesh::Halfedge_index> nm_vertices;
  CGAL::Polygon_mesh_processing::non_manifold_vertices(
      mesh, std::back_inserter(nm_vertices));
  if (!nm_vertices.empty()) {
    std::cout << label << " ASSERTION FAILED: mesh has " << nm_vertices.size()
              << " non-manifold vertices!" << std::endl;
    if (error_tokens) error_tokens->push_back("nonManifoldVertices");
    valid = false;
  }
  return valid;
}
