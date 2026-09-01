#include "test_base.h"
#include "fix/bridge.h"
#include "fix/repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

int main() {
    std::cout << "Testing Strategy II bridge_zero_volume_touches..." << std::endl;
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
    std::cout << "  - Valid input polygon mesh (separate descriptors)." << std::endl;

    // Detected as ambiguous due to spatial collision at (0, 0, 0)
    assert(!::jotcad::geo::fix::is_geometry_unambiguous(mesh));
    std::cout << "  - Detected as ambiguous touch at apex." << std::endl;

    // Apply bridge expansion
    bool bridged = ::jotcad::geo::fix::bridge_zero_volume_touches(mesh, EK::FT(1));
    std::cout << "  - bridge_zero_volume_touches executed (bridged = " << (bridged ? "true" : "false") << ")" << std::endl;

    std::cout << "✅ ALL bridge_test Passed." << std::endl;
    return 0;
}
