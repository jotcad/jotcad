#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
#include "kernel.h"
#include "fix/repair.h"
#include "boolean/engine.h"
#include <CGAL/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

using namespace jotcad::geo;
typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

int main() {
    std::string path = "scratch/self_touch_wedge.off";
    if (!std::filesystem::exists(path)) {
        std::cerr << "Fixture " << path << " not found!" << std::endl;
        return 1;
    }

    std::cout << "[Repair Wedge Test] Loading fixture: " << path << "..." << std::endl;
    Mesh mesh;
    if (!CGAL::IO::read_polygon_mesh(path, mesh)) {
        std::cerr << "Failed to read " << path << std::endl;
        return 1;
    }

    std::cout << "  - Initial Mesh: " << mesh.number_of_vertices() << " vertices, " 
              << mesh.number_of_faces() << " faces." << std::endl;
    std::cout << "  - is_closed: " << (CGAL::is_closed(mesh) ? "YES" : "NO") << std::endl;

    bool self_intersects_init = CGAL::Polygon_mesh_processing::does_self_intersect(mesh);
    std::cout << "  - Initial does_self_intersect: " << (self_intersects_init ? "YES" : "NO") << std::endl;

    std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> intersected_tris;
    CGAL::Polygon_mesh_processing::self_intersections(mesh, std::back_inserter(intersected_tris));
    std::cout << "  - Total intersecting face pairs: " << intersected_tris.size() << std::endl;

    for (size_t i = 0; i < (std::min)(size_t(10), intersected_tris.size()); ++i) {
        auto f1 = intersected_tris[i].first;
        auto f2 = intersected_tris[i].second;
        std::cout << "\n    Collision #" << (i + 1) << ": Face " << f1 << " vs Face " << f2 << std::endl;
        
        std::cout << "      Face " << f1 << ":";
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f1))) {
            auto p = mesh.point(v);
            std::cout << " v" << v << "(" << CGAL::to_double(p.x()) << ", " 
                      << CGAL::to_double(p.y()) << ", " << CGAL::to_double(p.z()) << ")";
        }
        std::cout << std::endl;

        std::cout << "      Face " << f2 << ":";
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f2))) {
            auto p = mesh.point(v);
            std::cout << " v" << v << "(" << CGAL::to_double(p.x()) << ", " 
                      << CGAL::to_double(p.y()) << ", " << CGAL::to_double(p.z()) << ")";
        }
        std::cout << std::endl;
    }

    // Step 1: Detect colliding coordinate groups
    std::map<EK::Point_3, std::vector<Mesh::Vertex_index>> coord_map;
    for (auto v : mesh.vertices()) coord_map[mesh.point(v)].push_back(v);

    std::cout << "\n[Pre-Repair Coordinate Collisions]:" << std::endl;
    for (auto const& [pt, vs] : coord_map) {
        if (vs.size() > 1) {
            std::cout << "  - Point (" << CGAL::to_double(pt.x()) << ", " 
                      << CGAL::to_double(pt.y()) << ", " << CGAL::to_double(pt.z()) << ") shared by " 
                      << vs.size() << " vertices: ";
            for (auto v : vs) std::cout << "v" << v << " ";
            std::cout << std::endl;

            for (auto v : vs) {
                std::cout << "    - Umbrella of v" << v << ": ";
                for (auto f : mesh.faces_around_target(mesh.halfedge(v))) {
                    std::cout << "f" << f << " ";
                }
                std::cout << std::endl;
            }
        }
    }

    // Test Repair
    std::cout << "\n[Applying Strategy I: make_geometry_unambiguous(0.01)]..." << std::endl;
    bool repaired = fix::make_geometry_unambiguous(mesh, EK::FT(1) / EK::FT(100));
    std::cout << "  - make_geometry_unambiguous returned: " << (repaired ? "MODIFIED" : "UNCHANGED") << std::endl;

    bool self_intersects_after = CGAL::Polygon_mesh_processing::does_self_intersect(mesh);
    std::cout << "  - After repair does_self_intersect: " << (self_intersects_after ? "YES" : "NO") << std::endl;

    std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> post_tris;
    CGAL::Polygon_mesh_processing::self_intersections(mesh, std::back_inserter(post_tris));
    std::cout << "  - Remaining intersecting face pairs: " << post_tris.size() << std::endl;

    for (size_t i = 0; i < (std::min)(size_t(5), post_tris.size()); ++i) {
        auto f1 = post_tris[i].first;
        auto f2 = post_tris[i].second;
        std::cout << "\n    Post Collision #" << (i + 1) << ": Face " << f1 << " vs Face " << f2 << std::endl;
        std::cout << "      Face " << f1 << ":";
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f1))) {
            auto p = mesh.point(v);
            std::cout << " v" << v << "(" << CGAL::to_double(p.x()) << ", " 
                      << CGAL::to_double(p.y()) << ", " << CGAL::to_double(p.z()) << ")";
        }
        std::cout << std::endl;
        std::cout << "      Face " << f2 << ":";
        for (auto v : mesh.vertices_around_face(mesh.halfedge(f2))) {
            auto p = mesh.point(v);
            std::cout << " v" << v << "(" << CGAL::to_double(p.x()) << ", " 
                      << CGAL::to_double(p.y()) << ", " << CGAL::to_double(p.z()) << ")";
        }
        std::cout << std::endl;
    }

    return 0;
}
