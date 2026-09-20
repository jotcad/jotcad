#pragma once

#include <CGAL/Surface_mesh.h>
#include <vector>
#include <set>
#include <map>
#include <iostream>
#include <cassert>
#include "kernel.h"
#include "geometry.h"

namespace jotcad {
namespace geo {

typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;

template <typename PointT>
inline PointT make_cgal_point(const Vertex& v);

template <>
inline EK::Point_3 make_cgal_point<EK::Point_3>(const Vertex& v) {
    return EK::Point_3(v.x, v.y, v.z);
}

template <>
inline IK::Point_3 make_cgal_point<IK::Point_3>(const Vertex& v) {
    return IK::Point_3(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
}

template <typename PointT>
inline Vertex make_geometry_vertex(const PointT& p);

template <>
inline Vertex make_geometry_vertex<EK::Point_3>(const EK::Point_3& p) {
    return Vertex{p.x(), p.y(), p.z()};
}

template <>
inline Vertex make_geometry_vertex<IK::Point_3>(const IK::Point_3& p) {
    return Vertex{FT(p.x()), FT(p.y()), FT(p.z())};
}

/**
 * to_surface_mesh:
 * Direct, faithful reconstruction of a CGAL Surface_mesh from a serialized Geometry struct.
 * Preserves coordinates and topology without treating Geometry as a soup.
 */
template <typename MeshT = ExactMesh>
inline MeshT to_surface_mesh(const Geometry& geo) {
    typedef typename MeshT::Point PointT;
    MeshT mesh;
    if (geo.vertices.empty()) return mesh;

    // Use triangles if present; otherwise, triangulate faces
    const Geometry* source = &geo;
    Geometry triangulated_storage;
    if (geo.triangles.empty() && !geo.faces.empty()) {
        triangulated_storage = geo;
        triangulated_storage.triangulate();
        source = &triangulated_storage;
    }

    std::vector<typename MeshT::Vertex_index> v_map;
    v_map.reserve(source->vertices.size());
    for (const auto& v : source->vertices) {
        v_map.push_back(mesh.add_vertex(make_cgal_point<PointT>(v)));
    }

    for (const auto& t : source->triangles) {
        if (t[0] == t[1] || t[1] == t[2] || t[2] == t[0]) continue;
        if (t[0] < 0 || t[0] >= (int)v_map.size() ||
            t[1] < 0 || t[1] >= (int)v_map.size() ||
            t[2] < 0 || t[2] >= (int)v_map.size()) continue;

        auto f = mesh.add_face(v_map[t[0]], v_map[t[1]], v_map[t[2]]);
        if (f == MeshT::null_face()) {
            f = mesh.add_face(v_map[t[0]], v_map[t[2]], v_map[t[1]]);
        }
        if (f == MeshT::null_face()) {
            std::cerr << "Failed to add triangle [" << t[0] << ", " << t[1] << ", " << t[2] << "]\n";
            std::cerr << "  v0: " << source->vertices[t[0]].x.exact() << ", " << source->vertices[t[0]].y.exact() << ", " << source->vertices[t[0]].z.exact() << "\n";
            std::cerr << "  v1: " << source->vertices[t[1]].x.exact() << ", " << source->vertices[t[1]].y.exact() << ", " << source->vertices[t[1]].z.exact() << "\n";
            std::cerr << "  v2: " << source->vertices[t[2]].x.exact() << ", " << source->vertices[t[2]].y.exact() << ", " << source->vertices[t[2]].z.exact() << "\n";
            auto check_e = [&](int i1, int i2) {
                auto h1 = mesh.halfedge(v_map[i1], v_map[i2]);
                auto h2 = mesh.halfedge(v_map[i2], v_map[i1]);
                std::cerr << "  edge (" << i1 << ", " << i2 << "): h(" << i1 << "->" << i2 << ")=" << (h1.is_valid() ? "valid" : "null")
                          << ", h(" << i2 << "->" << i1 << ")=" << (h2.is_valid() ? "valid" : "null") << "\n";
            };
            check_e(t[0], t[1]);
            check_e(t[1], t[2]);
            check_e(t[2], t[0]);
        }
        assert(f != MeshT::null_face() && "to_surface_mesh: failed to add face to mesh");
    }

    return mesh;
}

inline InexactMesh to_inexact_surface_mesh(const Geometry& geo) {
    return to_surface_mesh<InexactMesh>(geo);
}

/**
 * to_geometry:
 * Converts a CGAL Surface_mesh (ExactMesh or InexactMesh) into a JotCAD Geometry struct.
 */
template <typename MeshT>
inline Geometry to_geometry(const MeshT& mesh) {
    typedef typename MeshT::Point PointT;
    Geometry geo;
    std::map<typename MeshT::Vertex_index, int> v_map;
    for (auto v : mesh.vertices()) {
        v_map[v] = (int)geo.vertices.size();
        geo.vertices.push_back(make_geometry_vertex<PointT>(mesh.point(v)));
    }
    for (auto f : mesh.faces()) {
        std::vector<int> loop;
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]);
        assert(loop.size() == 3);
        geo.triangles.push_back({loop[0], loop[1], loop[2]});
    }
    return geo;
}

} // namespace geo
} // namespace jotcad
