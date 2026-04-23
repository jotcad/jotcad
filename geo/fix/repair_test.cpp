#include <iostream>
#include <vector>
#include <cassert>
#include "repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

void test_point_to_surface_collision() {
    std::cout << "[Test 5] Tetrahedron touching Cube facet centroid (Separate Indices)..." << std::endl;
    Mesh mesh;
    
    // Vertex 0: Centroid of a cube face
    auto v_cube_centroid = mesh.add_vertex(EK::Point_3(5, 5, 10));
    
    // Cube faces using v_cube_centroid
    auto v0 = mesh.add_vertex(EK::Point_3(0, 0, 10));
    auto v1 = mesh.add_vertex(EK::Point_3(10, 0, 10));
    auto v2 = mesh.add_vertex(EK::Point_3(10, 10, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(0, 10, 10));
    mesh.add_face(v0, v1, v_cube_centroid);
    mesh.add_face(v1, v2, v_cube_centroid);
    mesh.add_face(v2, v3, v_cube_centroid);
    mesh.add_face(v3, v0, v_cube_centroid);

    // Vertex 5: Apex of a tetrahedron at EXACTLY the same coordinate as v_cube_centroid
    auto v_tetra_apex = mesh.add_vertex(EK::Point_3(5, 5, 10));
    
    // Tetra faces using v_tetra_apex (pointing upwards)
    auto vt1 = mesh.add_vertex(EK::Point_3(3, 5, 15));
    auto vt2 = mesh.add_vertex(EK::Point_3(7, 3, 15));
    auto vt3 = mesh.add_vertex(EK::Point_3(7, 7, 15));
    mesh.add_face(v_tetra_apex, vt1, vt2);
    mesh.add_face(v_tetra_apex, vt2, vt3);
    mesh.add_face(v_tetra_apex, vt3, vt1);

    // Verify topological manifoldness
    assert(CGAL::is_valid_polygon_mesh(mesh));
    std::cout << "  ✅ Topologically valid (separate indices)." << std::endl;

    // Ambiguity state: Should be AMBIGUOUS because spatial merging would destroy the manifold.
    assert(!is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Detected as AMBIGUOUS (spatial collision @ face centroid)." << std::endl;

    // Resolve collision
    bool repaired = make_geometry_unambiguous(mesh, 0.1);
    assert(repaired);
    assert(is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Resolved: Apex and Centroid separated." << std::endl;
}

int main() {
    try {
        test_point_to_surface_collision();
        std::cout << "\nALL AMBIGUITY RESOLUTION TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
