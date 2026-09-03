#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <vector>
#include <set>
#include <map>
#include <cassert>
#include "kernel.h"
#include "geometry.h"
#include "fix/soup_repair.h"

namespace jotcad {
namespace geo {

typedef CGAL::Surface_mesh<EK::Point_3> ExactMesh;
typedef CGAL::Surface_mesh<IK::Point_3> InexactMesh;

/**
 * to_surface_mesh:
 * Converts a JotCAD Geometry struct into a CGAL ExactMesh (Surface_mesh<EK::Point_3>).
 */
inline ExactMesh to_surface_mesh(const Geometry& geo) {
    std::vector<EK::Point_3> pts;
    std::vector<std::vector<std::size_t>> faces;
    for (const auto& v : geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
    for (const auto& t : geo.triangles) faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
    for (const auto& f : geo.faces) {
        if (f.loops.empty()) continue;
        bool represented = false;
        if (!geo.triangles.empty()) {
            std::set<int> f_verts;
            for (const auto& loop : f.loops) {
                f_verts.insert(loop.begin(), loop.end());
            }
            for (const auto& t : geo.triangles) {
                if (f_verts.count(t[0]) && f_verts.count(t[1]) && f_verts.count(t[2])) {
                    represented = true;
                    break;
                }
            }
        }
        if (!represented) {
            std::vector<std::size_t> face;
            for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
            faces.push_back(face);
        }
    }
    fix::repair_solid_soup(pts, faces);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
    ExactMesh mesh;
    if (!faces.empty()) {
        if (!CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(faces)) {
            std::cerr << "❌ [MESH CONVERSION FAILED] to_surface_mesh: polygon soup does not define a valid 2-manifold polygon mesh!" << std::endl;
            assert(false && "to_surface_mesh: input polygon soup does not define a valid polygon mesh");
        }
        CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(pts, faces, mesh);
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    }
    return mesh;
}

/**
 * to_geometry:
 * Converts a CGAL ExactMesh (Surface_mesh<EK::Point_3>) into a JotCAD Geometry struct.
 */
inline Geometry to_geometry(const ExactMesh& mesh) {
    Geometry geo;
    std::map<ExactMesh::Vertex_index, int> v_map;
    for (auto v : mesh.vertices()) {
        v_map[v] = (int)geo.vertices.size();
        auto p = mesh.point(v);
        geo.vertices.push_back({p.x(), p.y(), p.z()});
    }
    for (auto f : mesh.faces()) {
        std::vector<int> loop;
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]);
        assert(loop.size() == 3);
        geo.triangles.push_back({loop[0], loop[1], loop[2]});
    }
    return geo;
}

/**
 * to_inexact_surface_mesh:
 * Converts a JotCAD Geometry struct into a CGAL InexactMesh (Surface_mesh<IK::Point_3>).
 */
inline InexactMesh to_inexact_surface_mesh(const Geometry& geo) {
    std::vector<IK::Point_3> pts;
    std::vector<std::vector<std::size_t>> faces;
    for (const auto& v : geo.vertices) pts.push_back(IK::Point_3(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)));
    for (const auto& t : geo.triangles) faces.push_back({(std::size_t)t[0], (std::size_t)t[1], (std::size_t)t[2]});
    for (const auto& f : geo.faces) {
        if (f.loops.empty()) continue;
        bool represented = false;
        if (!geo.triangles.empty()) {
            std::set<int> f_verts;
            for (const auto& loop : f.loops) {
                f_verts.insert(loop.begin(), loop.end());
            }
            for (const auto& t : geo.triangles) {
                if (f_verts.count(t[0]) && f_verts.count(t[1]) && f_verts.count(t[2])) {
                    represented = true;
                    break;
                }
            }
        }
        if (!represented) {
            std::vector<std::size_t> face;
            for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
            faces.push_back(face);
        }
    }
    fix::repair_solid_soup(pts, faces);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
    InexactMesh mesh;
    if (!faces.empty()) {
        if (!CGAL::Polygon_mesh_processing::is_polygon_soup_a_polygon_mesh(faces)) {
            std::cerr << "❌ [MESH CONVERSION FAILED] to_inexact_surface_mesh: polygon soup does not define a valid 2-manifold polygon mesh!" << std::endl;
            assert(false && "to_inexact_surface_mesh: input polygon soup does not define a valid polygon mesh");
        }
        CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(pts, faces, mesh);
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
    }
    return mesh;
}

/**
 * to_geometry:
 * Converts a CGAL InexactMesh (Surface_mesh<IK::Point_3>) into a JotCAD Geometry struct.
 */
inline Geometry to_geometry(const InexactMesh& mesh) {
    Geometry geo;
    std::map<InexactMesh::Vertex_index, int> v_map;
    for (auto v : mesh.vertices()) {
        v_map[v] = (int)geo.vertices.size();
        auto p = mesh.point(v);
        geo.vertices.push_back({CGAL::to_double(p.x()), CGAL::to_double(p.y()), CGAL::to_double(p.z())});
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
