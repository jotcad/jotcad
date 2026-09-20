#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <vector>
#include <iostream>
#include <cassert>
#include "kernel.h"
#include "fix/soup_repair.h"

namespace jotcad {
namespace geo {

typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;

/**
 * polygon_soup_to_surface_mesh_t:
 * Adapter converting raw, unindexed, or unoriented polygon soups
 * into a valid 2-manifold CGAL Surface_mesh via soup repair, orientation, and triangulation.
 */
template <typename MeshT>
inline MeshT polygon_soup_to_surface_mesh_t(
    std::vector<typename MeshT::Point>& points,
    std::vector<std::vector<std::size_t>>& polygons,
    bool repair = true
) {
    if (repair) {
        fix::repair_solid_soup(points, polygons);
        CGAL::Polygon_mesh_processing::repair_polygon_soup(points, polygons);
    }
    CGAL::Polygon_mesh_processing::orient_polygon_soup(points, polygons);
    MeshT mesh;
    if (!polygons.empty()) {
        if (!CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(polygons)) {
            std::cerr << "❌ [SOUP CONVERSION FAILED] polygon soup does not define a valid 2-manifold polygon mesh!" << std::endl;
            assert(false && "polygon soup does not define a valid polygon mesh");
        }
        CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(points, polygons, mesh);
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    }
    return mesh;
}

inline ExactMesh polygon_soup_to_surface_mesh(
    std::vector<EK::Point_3>& points,
    std::vector<std::vector<std::size_t>>& polygons,
    bool repair = true
) {
    return polygon_soup_to_surface_mesh_t<ExactMesh>(points, polygons, repair);
}

inline InexactMesh polygon_soup_to_inexact_surface_mesh(
    std::vector<IK::Point_3>& points,
    std::vector<std::vector<std::size_t>>& polygons,
    bool repair = true
) {
    return polygon_soup_to_surface_mesh_t<InexactMesh>(points, polygons, repair);
}

} // namespace geo
} // namespace jotcad
