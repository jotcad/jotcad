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

// Build minimal 12-vertex, 16-face kissing prisms fixture
Mesh build_minimal_kissing_prisms() {
    Mesh mesh;
    // Lobe 1 (Y < 0)
    auto v0 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-1, -1, 0));
    auto v2 = mesh.add_vertex(EK::Point_3(1, -1, 0));
    auto v3 = mesh.add_vertex(EK::Point_3(0, 0, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-1, -1, 10));
    auto v5 = mesh.add_vertex(EK::Point_3(1, -1, 10));

    mesh.add_face(v0, v2, v1); // Floor
    mesh.add_face(v3, v4, v5); // Ceiling
    mesh.add_face(v0, v3, v5); mesh.add_face(v0, v5, v2); // Right wall
    mesh.add_face(v2, v5, v4); mesh.add_face(v2, v4, v1); // Back wall
    mesh.add_face(v1, v4, v3); mesh.add_face(v1, v3, v0); // Left wall

    // Lobe 2 (Y > 0)
    auto v6  = mesh.add_vertex(EK::Point_3(0, 0, 0));  // Coincident with v0
    auto v7  = mesh.add_vertex(EK::Point_3(1, 1, 0));
    auto v8  = mesh.add_vertex(EK::Point_3(-1, 1, 0));
    auto v9  = mesh.add_vertex(EK::Point_3(0, 0, 10)); // Coincident with v3
    auto v10 = mesh.add_vertex(EK::Point_3(1, 1, 10));
    auto v11 = mesh.add_vertex(EK::Point_3(-1, 1, 10));

    mesh.add_face(v6, v8, v7); // Floor
    mesh.add_face(v9, v10, v11); // Ceiling
    mesh.add_face(v6, v9, v11); mesh.add_face(v6, v11, v8); // Left wall
    mesh.add_face(v8, v11, v10); mesh.add_face(v8, v10, v7); // Back wall
    mesh.add_face(v7, v10, v9); mesh.add_face(v7, v9, v6); // Right wall

    return mesh;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "TEST 1: Minimal Canonical Kissing Prisms (12 Vertices)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh min_mesh = build_minimal_kissing_prisms();
    std::cout << "  - Minimal Mesh: " << min_mesh.number_of_vertices() << " vertices, " 
              << min_mesh.number_of_faces() << " faces." << std::endl;
    std::cout << "  - is_closed: " << (CGAL::is_closed(min_mesh) ? "YES" : "NO") << std::endl;
    bool min_self_init = CGAL::Polygon_mesh_processing::does_self_intersect(min_mesh);
    std::cout << "  - Initial does_self_intersect: " << (min_self_init ? "YES" : "NO") << std::endl;

    std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> min_tris;
    CGAL::Polygon_mesh_processing::self_intersections(min_mesh, std::back_inserter(min_tris));
    std::cout << "  - Total intersecting face pairs: " << min_tris.size() << std::endl;

    std::cout << "\n[Applying Strategy I on Minimal Mesh]..." << std::endl;
    bool min_repaired = fix::make_geometry_unambiguous(min_mesh, EK::FT(1) / EK::FT(100));
    std::cout << "  - make_geometry_unambiguous returned: " << (min_repaired ? "MODIFIED" : "UNCHANGED") << std::endl;
    bool min_self_after = CGAL::Polygon_mesh_processing::does_self_intersect(min_mesh);
    std::cout << "  - After repair does_self_intersect: " << (min_self_after ? "YES" : "NO") << std::endl;
    std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> min_post_tris;
    CGAL::Polygon_mesh_processing::self_intersections(min_mesh, std::back_inserter(min_post_tris));
    std::cout << "  - Remaining intersecting face pairs: " << min_post_tris.size() << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 2: Real-World Bear Fixture (345 Vertices)" << std::endl;
    std::cout << "==================================================" << std::endl;
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
