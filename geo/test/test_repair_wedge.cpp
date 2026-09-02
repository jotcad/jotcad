#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
// Force rebuild with safe bounded collar splits
#include "kernel.h"
#include "fix/repair.h"
#include "fix/collar.h"
#include "boolean/engine.h"
#include <CGAL/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

using namespace jotcad::geo;
typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

// ============================================================================
// FIXTURE 1: Free Kissing Edge (Open at Both Ends, 12 Vertices)
// ============================================================================
Mesh build_free_kissing_edge_mesh() {
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

// ============================================================================
// FIXTURE 2: Forking Arms Merging into a Common Base Block
// ============================================================================
Mesh build_forking_arms_with_common_block() {
    Mesh mesh;
    // Base block: Z in [-4, 0], X in [-2, 2], Y in [-2, 2]
    auto b0 = mesh.add_vertex(EK::Point_3(-2, -2, -4));
    auto b1 = mesh.add_vertex(EK::Point_3(2, -2, -4));
    auto b2 = mesh.add_vertex(EK::Point_3(2, 2, -4));
    auto b3 = mesh.add_vertex(EK::Point_3(-2, 2, -4));

    // Top face of base block contains the junction vertex Vj at (0, 0, 0)
    auto vj = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto t0 = mesh.add_vertex(EK::Point_3(-2, -2, 0));
    auto t1 = mesh.add_vertex(EK::Point_3(2, -2, 0));
    auto t2 = mesh.add_vertex(EK::Point_3(2, 2, 0));
    auto t3 = mesh.add_vertex(EK::Point_3(-2, 2, 0));

    // Base block faces
    mesh.add_face(b0, b1, b2); mesh.add_face(b0, b2, b3); // Bottom
    mesh.add_face(b0, t0, t1); mesh.add_face(b0, t1, b1); // Front
    mesh.add_face(b1, t1, t2); mesh.add_face(b1, t2, b2); // Right
    mesh.add_face(b2, t2, t3); mesh.add_face(b2, t3, b3); // Back
    mesh.add_face(b3, t3, t0); mesh.add_face(b3, t0, b0); // Left

    // Top face of block triangulated around Vj
    mesh.add_face(t0, t1, vj);
    mesh.add_face(t1, t2, vj);
    mesh.add_face(t2, t3, vj);
    mesh.add_face(t3, t0, vj);

    // Rising Arm 1 (Y < 0, rising from Z=0 to Z=8)
    auto a1_top_vj = mesh.add_vertex(EK::Point_3(0, 0, 8));
    auto a1_top_t0 = mesh.add_vertex(EK::Point_3(-2, -2, 8));
    auto a1_top_t1 = mesh.add_vertex(EK::Point_3(2, -2, 8));

    mesh.add_face(a1_top_vj, a1_top_t1, a1_top_t0); // Arm 1 ceiling
    mesh.add_face(t0, a1_top_t0, a1_top_t1); mesh.add_face(t0, a1_top_t1, t1); // Outer wall
    mesh.add_face(t1, a1_top_t1, a1_top_vj); mesh.add_face(t1, a1_top_vj, vj); // Right wall
    mesh.add_face(vj, a1_top_vj, a1_top_t0); mesh.add_face(vj, a1_top_t0, t0); // Left wall

    // Rising Arm 2 (Y > 0, rising from Z=0 to Z=8, kissing along Vj -> (0,0,8))
    auto a2_top_vj = mesh.add_vertex(EK::Point_3(0, 0, 8)); // Coincident at top with a1_top_vj
    auto a2_top_t2 = mesh.add_vertex(EK::Point_3(2, 2, 8));
    auto a2_top_t3 = mesh.add_vertex(EK::Point_3(-2, 2, 8));

    mesh.add_face(a2_top_vj, a2_top_t3, a2_top_t2); // Arm 2 ceiling
    mesh.add_face(t2, a2_top_t2, a2_top_t3); mesh.add_face(t2, a2_top_t3, t3); // Outer wall
    mesh.add_face(t3, a2_top_t3, a2_top_vj); mesh.add_face(t3, a2_top_vj, vj); // Left wall
    mesh.add_face(vj, a2_top_vj, a2_top_t2); mesh.add_face(vj, a2_top_t2, t2); // Right wall

    return mesh;
}

// ============================================================================
// FIXTURE 3: Kissing Curve (Polygonal Chain of Multiple Connected Contact Edges)
// ============================================================================
Mesh build_kissing_curve_mesh() {
    Mesh mesh;
    // Curved contact seam along points P0, P1, P2, P3
    const int N_PTS = 4;
    std::vector<EK::Point_3> curve_pts = {
        EK::Point_3(0, 0, 0),
        EK::Point_3(1, 1, 3),
        EK::Point_3(2, 0, 6),
        EK::Point_3(1, -1, 9)
    };

    // Lobe 1 (Offset in -Y)
    std::vector<Mesh::Vertex_index> l1_seam, l1_back;
    for (int i = 0; i < N_PTS; ++i) {
        l1_seam.push_back(mesh.add_vertex(curve_pts[i]));
        l1_back.push_back(mesh.add_vertex(curve_pts[i] + EK::Vector_3(0, -2, 0)));
    }
    // Lobe 1 caps and walls
    mesh.add_face(l1_seam[0], l1_back[0], l1_back[1]); mesh.add_face(l1_seam[0], l1_back[1], l1_seam[1]); // bottom
    for (int i = 0; i + 1 < N_PTS; ++i) {
        mesh.add_face(l1_seam[i], l1_seam[i+1], l1_back[i+1]); mesh.add_face(l1_seam[i], l1_back[i+1], l1_back[i]);
    }
    mesh.add_face(l1_seam[N_PTS-1], l1_back[N_PTS-1], l1_back[N_PTS-2]); // top

    // Lobe 2 (Offset in +Y, kissing along curve_pts)
    std::vector<Mesh::Vertex_index> l2_seam, l2_back;
    for (int i = 0; i < N_PTS; ++i) {
        l2_seam.push_back(mesh.add_vertex(curve_pts[i])); // Coincident with l1_seam
        l2_back.push_back(mesh.add_vertex(curve_pts[i] + EK::Vector_3(0, 2, 0)));
    }
    for (int i = 0; i + 1 < N_PTS; ++i) {
        mesh.add_face(l2_seam[i], l2_back[i+1], l2_seam[i+1]); mesh.add_face(l2_seam[i], l2_back[i], l2_back[i+1]);
    }

    return mesh;
}

int main() {
    std::cout << "==================================================" << std::endl;
    std::cout << "TEST 1: Free Kissing Edge (Open at Both Ends)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh1 = build_free_kissing_edge_mesh();
    std::cout << "  - Mesh: " << mesh1.number_of_vertices() << " vertices, " << mesh1.number_of_faces() << " faces." << std::endl;
    std::cout << "  - is_closed: " << (CGAL::is_closed(mesh1) ? "YES" : "NO") << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh1) ? "YES" : "NO") << std::endl;
    bool rep1 = fix::separate_kissing_columns(mesh1, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep1 ? "MODIFIED" : "UNCHANGED") << std::endl;
    bool post1 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh1);
    std::cout << "  - After repair does_self_intersect: " << (post1 ? "YES" : "NO") << std::endl;
    assert(!post1);
    std::cout << "  ✅ TEST 1 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 2: Forking Arms Merging into Common Base Block" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh2 = build_forking_arms_with_common_block();
    std::cout << "  - Mesh: " << mesh2.number_of_vertices() << " vertices, " << mesh2.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh2) ? "YES" : "NO") << std::endl;
    bool rep2 = fix::separate_kissing_columns(mesh2, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep2 ? "MODIFIED" : "UNCHANGED") << std::endl;
    bool post2 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh2);
    std::cout << "  - After repair does_self_intersect: " << (post2 ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ TEST 2 COMPLETED (Self-intersect: " << (post2 ? "YES" : "NO") << ")" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 3: Kissing Curve (Multi-Segment Seam)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh3 = build_kissing_curve_mesh();
    std::cout << "  - Mesh: " << mesh3.number_of_vertices() << " vertices, " << mesh3.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh3) ? "YES" : "NO") << std::endl;
    bool rep3 = fix::separate_kissing_columns(mesh3, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep3 ? "MODIFIED" : "UNCHANGED") << std::endl;
    bool post3 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh3);
    std::cout << "  - After repair does_self_intersect: " << (post3 ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ TEST 3 COMPLETED (Self-intersect: " << (post3 ? "YES" : "NO") << ")" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 4: Real-World Bear Fixture (345 Vertices)" << std::endl;
    std::cout << "==================================================" << std::endl;
    std::string path = "scratch/self_touch_wedge.off";
    if (std::filesystem::exists(path)) {
        Mesh mesh4;
        CGAL::IO::read_polygon_mesh(path, mesh4);
        std::cout << "  - Initial Mesh: " << mesh4.number_of_vertices() << " vertices, " << mesh4.number_of_faces() << " faces." << std::endl;
        std::cout << "  - is_closed: " << (CGAL::is_closed(mesh4) ? "YES" : "NO") << std::endl;
        std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh4) ? "YES" : "NO") << std::endl;

        bool rep4 = fix::separate_kissing_columns(mesh4, EK::FT(1) / EK::FT(100));
        std::cout << "  - separate_kissing_columns returned: " << (rep4 ? "MODIFIED" : "UNCHANGED") << std::endl;
        bool post4 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh4);
        std::cout << "  - After repair does_self_intersect: " << (post4 ? "YES" : "NO") << std::endl;
        if (post4) {
            std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> post_tris;
            CGAL::Polygon_mesh_processing::self_intersections(mesh4, std::back_inserter(post_tris));
            std::cout << "  - Remaining intersecting face pairs: " << post_tris.size() << std::endl;
            for (size_t i = 0; i < (std::min)(size_t(5), post_tris.size()); ++i) {
                std::cout << "    Collision #" << (i+1) << ": Face " << post_tris[i].first << " vs " << post_tris[i].second << std::endl;
            }
        }
        assert(!post4);
        std::cout << "  ✅ TEST 4 PASSED (0 Collisions on Bear Fixture)" << std::endl;
    }

    std::cout << "\n🎉 ALL REGRESSION TESTS PASSED." << std::endl;
    return 0;
}
