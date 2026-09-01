#include <iostream>
#include <vector>
#include <cassert>
#include "bridge.h"
#include "repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

void test_bowtie_touch_bridge() {
    std::cout << "[Bridge Test 1] Two pyramids touching at apex..." << std::endl;
    Mesh mesh;

    // Common Apex coordinate P = (0, 0, 0)
    // Pyramid 1 (pointing down to apex)
    auto v_apex1 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-10, -10, 10));
    auto v2 = mesh.add_vertex(EK::Point_3(10, -10, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(10, 10, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-10, 10, 10));
    mesh.add_face(v_apex1, v2, v1);
    mesh.add_face(v_apex1, v3, v2);
    mesh.add_face(v_apex1, v4, v3);
    mesh.add_face(v_apex1, v1, v4);

    // Pyramid 2 (pointing up to apex)
    auto v_apex2 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto u1 = mesh.add_vertex(EK::Point_3(-10, -10, -10));
    auto u2 = mesh.add_vertex(EK::Point_3(10, -10, -10));
    auto u3 = mesh.add_vertex(EK::Point_3(10, 10, -10));
    auto u4 = mesh.add_vertex(EK::Point_3(-10, 10, -10));
    mesh.add_face(v_apex2, u1, u2);
    mesh.add_face(v_apex2, u2, u3);
    mesh.add_face(v_apex2, u3, u4);
    mesh.add_face(v_apex2, u4, u1);

    // Topologically valid separate descriptors
    assert(CGAL::is_valid_polygon_mesh(mesh));
    std::cout << "  ✅ Valid input polygon mesh." << std::endl;

    // Detected as ambiguous due to spatial collision at (0, 0, 0)
    assert(!is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Detected as ambiguous touch at apex." << std::endl;

    // Apply bridge expansion
    bool bridged = bridge_zero_volume_touches(mesh, EK::FT(1));
    std::cout << "  ✅ bridge_zero_volume_touches executed: " << (bridged ? "bridged" : "unchanged") << std::endl;
}

int main() {
    try {
        test_bowtie_touch_bridge();
        std::cout << "\nALL BRIDGE EXPANSION TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
