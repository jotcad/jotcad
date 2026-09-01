#include "test_base.h"
#include "fix/repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

int main() {
    std::cout << "Testing Strategy I make_geometry_unambiguous..." << std::endl;
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
    std::cout << "  - Topologically valid (separate indices)." << std::endl;

    // Ambiguity state: Should be AMBIGUOUS because spatial merging would destroy the manifold.
    assert(!::jotcad::geo::fix::is_geometry_unambiguous(mesh));
    std::cout << "  - Detected as AMBIGUOUS (spatial collision @ face centroid)." << std::endl;

    // Resolve collision
    bool repaired = ::jotcad::geo::fix::make_geometry_unambiguous(mesh, EK::FT(1) / EK::FT(10));
    assert(repaired);
    assert(::jotcad::geo::fix::is_geometry_unambiguous(mesh));
    std::cout << "  - Resolved: Apex and Centroid separated." << std::endl;

    // Test on real-world extruded wedge fixture if present
    if (std::filesystem::exists("scratch/self_touch_wedge.off")) {
        std::cout << "  - Testing real-world fixture scratch/self_touch_wedge.off..." << std::endl;
        Mesh wedge;
        CGAL::IO::read_polygon_mesh("scratch/self_touch_wedge.off", wedge);
        assert(CGAL::is_valid_polygon_mesh(wedge));
        
        bool self_intersects_before = CGAL::Polygon_mesh_processing::does_self_intersect(wedge);
        std::cout << "    - Before repair self_intersects: " << (self_intersects_before ? "YES" : "NO") << std::endl;
        
        bool repaired_wedge = ::jotcad::geo::fix::make_geometry_unambiguous(wedge, EK::FT(1) / EK::FT(100));
        std::cout << "    - make_geometry_unambiguous result: " << (repaired_wedge ? "MODIFIED" : "UNCHANGED") << std::endl;
        
        bool self_intersects_after = CGAL::Polygon_mesh_processing::does_self_intersect(wedge);
        std::cout << "    - After repair self_intersects: " << (self_intersects_after ? "YES" : "NO") << std::endl;
    }

    std::cout << "✅ ALL repair_test Passed." << std::endl;
    return 0;
}
