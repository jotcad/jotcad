#include <iostream>
#include <vector>
#include <cassert>
#include "repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

void test_edge_touch_aligned() {
    std::cout << "[Test 3] Aligned Edge-Touching Cubes..." << std::endl;
    Mesh mesh;
    auto v_e1 = mesh.add_vertex(EK::Point_3(10, 10, 0));
    auto v_e2 = mesh.add_vertex(EK::Point_3(10, 10, 10));

    auto v1 = mesh.add_vertex(EK::Point_3(0, 10, 0));
    auto v2 = mesh.add_vertex(EK::Point_3(0, 10, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(10, 0, 0));
    auto v4 = mesh.add_vertex(EK::Point_3(10, 0, 10));
    mesh.add_face(v_e1, v_e2, v2); mesh.add_face(v_e1, v2, v1);
    mesh.add_face(v_e1, v3, v4); mesh.add_face(v_e1, v4, v_e2);

    auto v5 = mesh.add_vertex(EK::Point_3(20, 10, 0));
    auto v6 = mesh.add_vertex(EK::Point_3(20, 10, 10));
    auto v7 = mesh.add_vertex(EK::Point_3(10, 20, 0));
    auto v8 = mesh.add_vertex(EK::Point_3(10, 20, 10));
    mesh.add_face(v_e2, v_e1, v6); mesh.add_face(v_e1, v5, v6);
    mesh.add_face(v_e1, v_e2, v8); mesh.add_face(v_e2, v7, v8);

    assert(!is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Geometry detected as AMBIGUOUS (expected)." << std::endl;

    bool repaired = make_geometry_unambiguous(mesh, 0.1);
    assert(repaired);
    assert(is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Geometry is now UNAMBIGUOUS." << std::endl;
}

void test_edge_touch_unaligned() {
    std::cout << "[Test 4] Unaligned Edge-Touching (T-Junction)..." << std::endl;
    Mesh mesh;
    auto v_shared = mesh.add_vertex(EK::Point_3(10, 10, 5));
    auto v1 = mesh.add_vertex(EK::Point_3(0, 10, 0));
    auto v2 = mesh.add_vertex(EK::Point_3(20, 10, 0));
    mesh.add_face(v_shared, v2, v1);
    auto v3 = mesh.add_vertex(EK::Point_3(10, 0, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(10, 20, 10));
    mesh.add_face(v_shared, v3, v4);

    assert(!is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Geometry detected as AMBIGUOUS (expected)." << std::endl;

    make_geometry_unambiguous(mesh, 0.1);
    assert(is_geometry_unambiguous(mesh));
    std::cout << "  ✅ Geometry is now UNAMBIGUOUS." << std::endl;
}

int main() {
    try {
        test_edge_touch_aligned();
        test_edge_touch_unaligned();
        std::cout << "\nALL AMBIGUITY RESOLUTION TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
