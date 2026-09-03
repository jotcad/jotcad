#pragma once
#include <cassert>
#include <iostream>
#include <string>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/boost/graph/helpers.h>

namespace jotcad {
namespace geo {
namespace fix {

// Mesh validity status enum ordered by failure condition
enum class MeshStatus {
    OK = 0,
    HAS_GARBAGE,
    EMPTY,
    NOT_CLOSED,
    NOT_TRIANGULATED,
    INVALID_CONNECTIVITY,
    DOES_NOT_BOUND_VOLUME,
    SELF_INTERSECTING
};

inline const char* to_string(MeshStatus status) {
    switch (status) {
        case MeshStatus::OK: return "OK";
        case MeshStatus::HAS_GARBAGE: return "mesh has uncollected garbage elements";
        case MeshStatus::EMPTY: return "mesh is empty (0 vertices or 0 faces)";
        case MeshStatus::NOT_CLOSED: return "mesh is not a closed 2-manifold (contains open boundary border halfedges)";
        case MeshStatus::NOT_TRIANGULATED: return "mesh contains non-triangular faces";
        case MeshStatus::INVALID_CONNECTIVITY: return "mesh has combinatorially invalid halfedge graph";
        case MeshStatus::DOES_NOT_BOUND_VOLUME: return "mesh does not bound a positive volume (normals/orientation invalid)";
        case MeshStatus::SELF_INTERSECTING: return "mesh contains self-intersections or self-touching geometry";
        default: return "unknown mesh error";
    }
}

// 1. Corefinement Preconditions: Pure topological correctness (checks 1-5 in increasing cost order)
template <typename Mesh>
inline MeshStatus check_corefinement_preconditions(const Mesh& m) {
    // 1. O(1) Memory flag check
    if (m.has_garbage()) return MeshStatus::HAS_GARBAGE;

    // 2. O(1) Size check
    if (m.number_of_vertices() == 0 || m.number_of_faces() == 0) return MeshStatus::EMPTY;

    // 3. O(1)-O(N) Early-exiting border halfedge scan
    if (!CGAL::is_closed(m)) return MeshStatus::NOT_CLOSED;

    // 4. O(1)-O(N) Early-exiting face degree scan
    if (!CGAL::is_triangle_mesh(m)) return MeshStatus::NOT_TRIANGULATED;

    // 5. O(N) Exhaustive combinatorial graph audit
    if (!m.is_valid() || !CGAL::is_valid_polygon_mesh(m)) return MeshStatus::INVALID_CONNECTIVITY;

    return MeshStatus::OK;
}

// 2. Solid Mesh Invariants: Full topological & geometric correctness (checks 1-7 in increasing cost order)
template <typename Mesh>
inline MeshStatus check_solid_mesh(const Mesh& m) {
    // Run topological precondition checks (1-5) first
    MeshStatus status = check_corefinement_preconditions(m);
    if (status != MeshStatus::OK) return status;

    // 6. O(N) Volume enclosure & normal orientation test
    if (!CGAL::Polygon_mesh_processing::does_bound_a_volume(m)) {
        return MeshStatus::DOES_NOT_BOUND_VOLUME;
    }

    // 7. O(N log N) Exact rational geometric collision / self-intersection test
    if (CGAL::Polygon_mesh_processing::does_self_intersect(m)) {
        return MeshStatus::SELF_INTERSECTING;
    }

    return MeshStatus::OK;
}

// --- Queryable Boolean Predicates ---

template <typename Mesh>
inline bool is_well_formed_for_corefinement(const Mesh& m) {
    return check_corefinement_preconditions(m) == MeshStatus::OK;
}

template <typename Mesh>
inline bool is_well_formed_mesh(const Mesh& m) {
    return check_solid_mesh(m) == MeshStatus::OK;
}

template <typename Mesh>
inline bool is_closed_2manifold(const Mesh& m) {
    return !m.has_garbage() && m.number_of_vertices() > 0 && m.number_of_faces() > 0 && CGAL::is_closed(m) && m.is_valid();
}

template <typename Mesh>
inline bool is_watertight_solid(const Mesh& m) {
    return is_well_formed_mesh(m);
}

// --- Hard Invariant Assertions ---

template <typename Mesh>
inline void assert_well_formed_for_corefinement(const Mesh& m, const std::string& label) {
    MeshStatus status = check_corefinement_preconditions(m);
    if (status != MeshStatus::OK) {
        std::cerr << "❌ [TOPOLOGY FAILED] " << label << ": " << to_string(status) << "!" << std::endl;
        assert(false && "Mesh failed topological corefinement precondition");
    }
}

template <typename Mesh>
inline void assert_well_formed_mesh(const Mesh& m, const std::string& label) {
    MeshStatus status = check_solid_mesh(m);
    if (status != MeshStatus::OK) {
        std::cerr << "❌ [GEOMETRY FAILED] " << label << ": " << to_string(status) << "!" << std::endl;
        assert(false && "Mesh failed solid geometry assertion");
    }
}

} // namespace fix
} // namespace geo
} // namespace jotcad
