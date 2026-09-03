#pragma once
#include <cassert>
#include <iostream>
#include <string>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

namespace jotcad {
namespace geo {
namespace fix {

// Pure topological correctness required for corefinement
template <typename Mesh>
inline void assert_well_formed_for_corefinement(const Mesh& m, const std::string& label) {
    if (m.has_garbage()) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << " has uncollected garbage!" << std::endl;
        assert(!m.has_garbage());
    }
    if (m.number_of_vertices() == 0 || m.number_of_faces() == 0) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << " is empty (vertices="
                  << m.number_of_vertices() << ", faces=" << m.number_of_faces() << ")!" << std::endl;
        assert(m.number_of_vertices() > 0 && m.number_of_faces() > 0);
    }
    if (!m.is_valid()) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << " has combinatorially invalid halfedge graph!" << std::endl;
        assert(m.is_valid());
    }
    if (!CGAL::is_closed(m)) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << " is not a closed 2-manifold (contains open boundary border halfedges)!" << std::endl;
        assert(CGAL::is_closed(m));
    }
    if (!CGAL::is_triangle_mesh(m)) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << " contains non-triangular faces!" << std::endl;
        assert(CGAL::is_triangle_mesh(m));
    }
}

// Geometric and topological well-formedness for completed solid meshes
template <typename Mesh>
inline void assert_well_formed_mesh(const Mesh& m, const std::string& label) {
    if (m.number_of_vertices() == 0 || m.number_of_faces() == 0) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " is empty (vertices="
                  << m.number_of_vertices() << ", faces=" << m.number_of_faces() << ")!" << std::endl;
        assert(m.number_of_vertices() > 0 && m.number_of_faces() > 0);
    }
    if (!m.is_valid()) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " has combinatorially invalid halfedge graph!" << std::endl;
        assert(m.is_valid());
    }
    if (!CGAL::is_closed(m)) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " is NOT a closed 2-manifold!" << std::endl;
        assert(CGAL::is_closed(m));
    }
    if (!CGAL::is_triangle_mesh(m)) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " contains non-triangular faces!" << std::endl;
        assert(CGAL::is_triangle_mesh(m));
    }
    if (CGAL::Polygon_mesh_processing::does_self_intersect(m)) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " has SELF-INTERSECTIONS!" << std::endl;
        assert(!CGAL::Polygon_mesh_processing::does_self_intersect(m));
    }
    if (!CGAL::Polygon_mesh_processing::does_bound_a_volume(m)) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << " DOES NOT bound a volume! (Orientation/normals invalid)" << std::endl;
        assert(CGAL::Polygon_mesh_processing::does_bound_a_volume(m));
    }
}

} // namespace fix
} // namespace geo
} // namespace jotcad
