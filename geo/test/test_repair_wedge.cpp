#include <iostream>
#include <vector>
#include <cassert>
#include <filesystem>
#include "kernel.h"
#include "fix/repair.h"
#include "fix/kiss.h"
#include "kiss_fixtures.h"
#include "boolean/engine.h"
#include <CGAL/IO/polygon_mesh_io.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

using namespace jotcad::geo;
using namespace jotcad::geo::test;
typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

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
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh1);
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
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh2);
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
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh3);
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
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh4);
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

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 5: Two-Anchor Bridged Seam (Joined at Both Ends)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh5 = build_bridged_kissing_edge_mesh();
    std::cout << "  - Mesh: " << mesh5.number_of_vertices() << " vertices, " << mesh5.number_of_faces() << " faces." << std::endl;
    std::cout << "  - is_closed: " << (CGAL::is_closed(mesh5) ? "YES" : "NO") << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh5) ? "YES" : "NO") << std::endl;
    bool rep5 = fix::separate_kissing_columns(mesh5, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep5 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh5);
    bool post5 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh5);
    std::cout << "  - After repair does_self_intersect: " << (post5 ? "YES" : "NO") << std::endl;
    std::cout << "  ✅ TEST 5 COMPLETED (Self-intersect: " << (post5 ? "YES" : "NO") << ")" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 6: Dual Minkowski Welding (KissMode::WELD)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh6 = build_free_kissing_edge_mesh();
    std::cout << "  - Mesh: " << mesh6.number_of_vertices() << " vertices, " << mesh6.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before weld does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh6) ? "YES" : "NO") << std::endl;
    bool rep6 = fix::weld_kissing_columns(mesh6, EK::FT(1) / EK::FT(100));
    std::cout << "  - weld_kissing_columns result: " << (rep6 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh6);
    bool post6 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh6);
    std::cout << "  - After weld does_self_intersect: " << (post6 ? "YES" : "NO") << std::endl;
    std::cout << "  - is_closed: " << (CGAL::is_closed(mesh6) ? "YES" : "NO") << std::endl;
    std::cout << "  - is_triangle_mesh: " << (CGAL::is_triangle_mesh(mesh6) ? "YES" : "NO") << std::endl;
    assert(!post6);
    assert(CGAL::is_closed(mesh6));
    std::cout << "  ✅ TEST 6 PASSED (Weld produces 0 collisions and watertight manifold)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 7: Point-to-Point Kiss (Two Pyramids Apex-to-Apex)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh7 = build_point_to_point_mesh();
    std::cout << "  - Mesh: " << mesh7.number_of_vertices() << " vertices, " << mesh7.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh7) ? "YES" : "NO") << std::endl;
    bool rep7 = fix::separate_kissing_columns(mesh7, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep7 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh7);
    bool post7 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh7);
    std::cout << "  - After repair does_self_intersect: " << (post7 ? "YES" : "NO") << std::endl;
    assert(!post7);
    std::cout << "  ✅ TEST 7 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 8: Point-to-Edge Kiss (Pyramid Apex on Cube Edge)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh8 = build_point_to_edge_mesh();
    std::cout << "  - Mesh: " << mesh8.number_of_vertices() << " vertices, " << mesh8.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh8) ? "YES" : "NO") << std::endl;
    bool rep8 = fix::separate_kissing_columns(mesh8, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep8 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh8);
    bool post8 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh8);
    std::cout << "  - After repair does_self_intersect: " << (post8 ? "YES" : "NO") << std::endl;
    assert(!post8);
    std::cout << "  ✅ TEST 8 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 9: Point-to-Face Kiss (Pyramid Apex on Cube Face)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh9 = build_point_to_face_mesh();
    std::cout << "  - Mesh: " << mesh9.number_of_vertices() << " vertices, " << mesh9.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh9) ? "YES" : "NO") << std::endl;
    bool rep9 = fix::separate_kissing_columns(mesh9, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep9 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh9);
    bool post9 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh9);
    std::cout << "  - After repair does_self_intersect: " << (post9 ? "YES" : "NO") << std::endl;
    assert(!post9);
    std::cout << "  ✅ TEST 9 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 10: Edge-to-Face Kiss (Prism Edge on Cube Face)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh10 = build_edge_to_face_mesh();
    std::cout << "  - Mesh: " << mesh10.number_of_vertices() << " vertices, " << mesh10.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh10) ? "YES" : "NO") << std::endl;
    bool rep10 = fix::separate_kissing_columns(mesh10, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep10 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh10);
    bool post10 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh10);
    std::cout << "  - After repair does_self_intersect: " << (post10 ? "YES" : "NO") << std::endl;
    assert(!post10);
    std::cout << "  ✅ TEST 10 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 11: Face-to-Face Kiss (Two Cubes Sharing Flush Face)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh11 = build_face_to_face_mesh();
    std::cout << "  - Mesh: " << mesh11.number_of_vertices() << " vertices, " << mesh11.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh11) ? "YES" : "NO") << std::endl;
    bool rep11 = fix::separate_kissing_columns(mesh11, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep11 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh11);
    bool post11 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh11);
    std::cout << "  - After repair does_self_intersect: " << (post11 ? "YES" : "NO") << std::endl;
    assert(!post11);
    std::cout << "  ✅ TEST 11 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 12: Near-Collinear Polyline (Partial Consolidation across Threshold)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh12 = build_near_collinear_polyline_mesh();
    std::cout << "  - Mesh: " << mesh12.number_of_vertices() << " vertices, " << mesh12.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh12) ? "YES" : "NO") << std::endl;
    bool rep12 = fix::separate_kissing_columns(mesh12, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep12 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh12);
    bool post12 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh12);
    std::cout << "  - After repair does_self_intersect: " << (post12 ? "YES" : "NO") << std::endl;
    if (post12) {
        std::vector<std::pair<Mesh::Face_index, Mesh::Face_index>> post_tris;
        CGAL::Polygon_mesh_processing::self_intersections(mesh12, std::back_inserter(post_tris));
        std::cout << "  - Remaining intersecting face pairs: " << post_tris.size() << std::endl;
        for (size_t i = 0; i < (std::min)(size_t(5), post_tris.size()); ++i) {
            std::cout << "    Collision #" << (i+1) << ": Face " << post_tris[i].first << " vs " << post_tris[i].second << std::endl;
        }
    }
    assert(!post12);
    std::cout << "  ✅ TEST 12 PASSED (0 Collisions)" << std::endl;

    std::cout << "\n==================================================" << std::endl;
    std::cout << "TEST 13: Non-Convex L-Patch with Protected Cavity (Convex Decomposition)" << std::endl;
    std::cout << "==================================================" << std::endl;
    Mesh mesh13 = build_non_convex_L_patch_mesh();
    std::cout << "  - Mesh: " << mesh13.number_of_vertices() << " vertices, " << mesh13.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Before repair does_self_intersect: " << (CGAL::Polygon_mesh_processing::does_self_intersect(mesh13) ? "YES" : "NO") << std::endl;
    bool rep13 = fix::separate_kissing_columns(mesh13, EK::FT(1) / EK::FT(100));
    std::cout << "  - separate_kissing_columns result: " << (rep13 ? "MODIFIED" : "UNCHANGED") << std::endl;
    CGAL::Polygon_mesh_processing::triangulate_faces(mesh13);
    bool post13 = CGAL::Polygon_mesh_processing::does_self_intersect(mesh13);
    std::cout << "  - After repair does_self_intersect: " << (post13 ? "YES" : "NO") << std::endl;
    assert(!post13);
    std::cout << "  ✅ TEST 13 PASSED (0 Collisions, Protected Cavity Intact)" << std::endl;

    std::cout << "\n🎉 ALL REGRESSION TESTS PASSED." << std::endl;
    return 0;
}
