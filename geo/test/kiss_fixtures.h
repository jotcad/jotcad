#pragma once
#include "kernel.h"
#include "fix/tool_builder.h"
#include <CGAL/Surface_mesh.h>
#include <vector>

namespace jotcad {
namespace geo {
namespace test {

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

// ============================================================================
// FIXTURE 1: Free Kissing Edge (Open at Both Ends, 12 Vertices)
// ============================================================================
inline Mesh build_free_kissing_edge_mesh() {
    Mesh mesh;
    auto v0 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-1, -1, 0));
    auto v2 = mesh.add_vertex(EK::Point_3(1, -1, 0));
    auto v3 = mesh.add_vertex(EK::Point_3(0, 0, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-1, -1, 10));
    auto v5 = mesh.add_vertex(EK::Point_3(1, -1, 10));

    mesh.add_face(v0, v2, v1);
    mesh.add_face(v3, v4, v5);
    mesh.add_face(v0, v3, v5); mesh.add_face(v0, v5, v2);
    mesh.add_face(v2, v5, v4); mesh.add_face(v2, v4, v1);
    mesh.add_face(v1, v4, v3); mesh.add_face(v1, v3, v0);

    auto v6  = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v7  = mesh.add_vertex(EK::Point_3(1, 1, 0));
    auto v8  = mesh.add_vertex(EK::Point_3(-1, 1, 0));
    auto v9  = mesh.add_vertex(EK::Point_3(0, 0, 10));
    auto v10 = mesh.add_vertex(EK::Point_3(1, 1, 10));
    auto v11 = mesh.add_vertex(EK::Point_3(-1, 1, 10));

    mesh.add_face(v6, v8, v7);
    mesh.add_face(v9, v10, v11);
    mesh.add_face(v6, v9, v11); mesh.add_face(v6, v11, v8);
    mesh.add_face(v8, v11, v10); mesh.add_face(v8, v10, v7);
    mesh.add_face(v7, v10, v9); mesh.add_face(v7, v9, v6);
    return mesh;
}

// ============================================================================
// FIXTURE 2: Forking Arms Merging into a Common Base Block
// ============================================================================
inline Mesh build_forking_arms_with_common_block() {
    Mesh mesh;
    auto b0 = mesh.add_vertex(EK::Point_3(-2, -2, -4));
    auto b1 = mesh.add_vertex(EK::Point_3(2, -2, -4));
    auto b2 = mesh.add_vertex(EK::Point_3(2, 2, -4));
    auto b3 = mesh.add_vertex(EK::Point_3(-2, 2, -4));

    auto vj = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto t0 = mesh.add_vertex(EK::Point_3(-2, -2, 0));
    auto t1 = mesh.add_vertex(EK::Point_3(2, -2, 0));
    auto t2 = mesh.add_vertex(EK::Point_3(2, 2, 0));
    auto t3 = mesh.add_vertex(EK::Point_3(-2, 2, 0));

    mesh.add_face(b0, b1, b2); mesh.add_face(b0, b2, b3);
    mesh.add_face(b0, t0, t1); mesh.add_face(b0, t1, b1);
    mesh.add_face(b1, t1, t2); mesh.add_face(b1, t2, b2);
    mesh.add_face(b2, t2, t3); mesh.add_face(b2, t3, b3);
    mesh.add_face(b3, t3, t0); mesh.add_face(b3, t0, b0);

    mesh.add_face(t0, t1, vj);
    mesh.add_face(t1, t2, vj);
    mesh.add_face(t2, t3, vj);
    mesh.add_face(t3, t0, vj);

    auto a1_top_vj = mesh.add_vertex(EK::Point_3(0, 0, 8));
    auto a1_top_t0 = mesh.add_vertex(EK::Point_3(-2, -2, 8));
    auto a1_top_t1 = mesh.add_vertex(EK::Point_3(2, -2, 8));

    mesh.add_face(a1_top_vj, a1_top_t1, a1_top_t0);
    mesh.add_face(t0, a1_top_t0, a1_top_t1); mesh.add_face(t0, a1_top_t1, t1);
    mesh.add_face(t1, a1_top_t1, a1_top_vj); mesh.add_face(t1, a1_top_vj, vj);
    mesh.add_face(vj, a1_top_vj, a1_top_t0); mesh.add_face(vj, a1_top_t0, t0);

    auto a2_top_vj = mesh.add_vertex(EK::Point_3(0, 0, 8));
    auto a2_top_t2 = mesh.add_vertex(EK::Point_3(2, 2, 8));
    auto a2_top_t3 = mesh.add_vertex(EK::Point_3(-2, 2, 8));

    mesh.add_face(a2_top_vj, a2_top_t3, a2_top_t2);
    mesh.add_face(t2, a2_top_t2, a2_top_t3); mesh.add_face(t2, a2_top_t3, t3);
    mesh.add_face(t3, a2_top_t3, a2_top_vj); mesh.add_face(t3, a2_top_vj, vj);
    mesh.add_face(vj, a2_top_vj, a2_top_t2); mesh.add_face(vj, a2_top_t2, t2);
    return mesh;
}

// ============================================================================
// FIXTURE 3: Kissing Curve (Polygonal Chain of Multiple Connected Edges)
// ============================================================================
inline Mesh build_kissing_curve_mesh() {
    Mesh mesh;
    const int N_PTS = 4;
    std::vector<EK::Point_3> curve_pts = {
        EK::Point_3(0, 0, 0),
        EK::Point_3(1, 1, 3),
        EK::Point_3(2, 0, 6),
        EK::Point_3(1, -1, 9)
    };

    std::vector<Mesh::Vertex_index> l1_seam, l1_back;
    for (int i = 0; i < N_PTS; ++i) {
        l1_seam.push_back(mesh.add_vertex(curve_pts[i]));
        l1_back.push_back(mesh.add_vertex(curve_pts[i] + EK::Vector_3(0, -2, 0)));
    }
    mesh.add_face(l1_seam[0], l1_back[0], l1_back[1]); mesh.add_face(l1_seam[0], l1_back[1], l1_seam[1]);
    for (int i = 0; i + 1 < N_PTS; ++i) {
        mesh.add_face(l1_seam[i], l1_seam[i+1], l1_back[i+1]); mesh.add_face(l1_seam[i], l1_back[i+1], l1_back[i]);
    }
    mesh.add_face(l1_seam[N_PTS-1], l1_back[N_PTS-1], l1_back[N_PTS-2]);

    std::vector<Mesh::Vertex_index> l2_seam, l2_back;
    for (int i = 0; i < N_PTS; ++i) {
        l2_seam.push_back(mesh.add_vertex(curve_pts[i]));
        l2_back.push_back(mesh.add_vertex(curve_pts[i] + EK::Vector_3(0, 2, 0)));
    }
    for (int i = 0; i + 1 < N_PTS; ++i) {
        mesh.add_face(l2_seam[i], l2_back[i+1], l2_seam[i+1]); mesh.add_face(l2_seam[i], l2_back[i], l2_back[i+1]);
    }
    return mesh;
}

// ============================================================================
// FIXTURE 5: Two-Anchor Bridged Seam
// ============================================================================
inline Mesh build_bridged_kissing_edge_mesh() {
    Mesh mesh;
    auto v_bot = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v_top = mesh.add_vertex(EK::Point_3(0, 0, 10));

    auto a1_b0 = mesh.add_vertex(EK::Point_3(-2, -2, 0));
    auto a1_b1 = mesh.add_vertex(EK::Point_3(2, -2, 0));
    auto a1_t0 = mesh.add_vertex(EK::Point_3(-2, -2, 10));
    auto a1_t1 = mesh.add_vertex(EK::Point_3(2, -2, 10));

    mesh.add_face(v_bot, a1_b0, a1_b1);
    mesh.add_face(v_top, a1_t1, a1_t0);
    mesh.add_face(a1_b0, a1_t0, a1_t1); mesh.add_face(a1_b0, a1_t1, a1_b1);
    mesh.add_face(a1_b1, a1_t1, v_top); mesh.add_face(a1_b1, v_top, v_bot);
    mesh.add_face(v_bot, v_top, a1_t0); mesh.add_face(v_bot, a1_t0, a1_b0);

    auto a2_b0 = mesh.add_vertex(EK::Point_3(2, 2, 0));
    auto a2_b1 = mesh.add_vertex(EK::Point_3(-2, 2, 0));
    auto a2_t0 = mesh.add_vertex(EK::Point_3(2, 2, 10));
    auto a2_t1 = mesh.add_vertex(EK::Point_3(-2, 2, 10));

    mesh.add_face(v_bot, a2_b0, a2_b1);
    mesh.add_face(v_top, a2_t1, a2_t0);
    mesh.add_face(a2_b0, a2_t0, a2_t1); mesh.add_face(a2_b0, a2_t1, a2_b1);
    mesh.add_face(a2_b1, a2_t1, v_top); mesh.add_face(a2_b1, v_top, v_bot);
    mesh.add_face(v_bot, v_top, a2_t0); mesh.add_face(v_bot, a2_t0, a2_b0);
    return mesh;
}

// ============================================================================
// FIXTURE 7: Point-to-Point (Two Pyramids Apex-to-Apex)
// ============================================================================
inline Mesh build_point_to_point_mesh() {
    Mesh mesh;
    auto v_apex1 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-5, -5, 10));
    auto v2 = mesh.add_vertex(EK::Point_3(5, -5, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(5, 5, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-5, 5, 10));
    mesh.add_face(v1, v2, v3); mesh.add_face(v1, v3, v4);
    mesh.add_face(v_apex1, v2, v1);
    mesh.add_face(v_apex1, v3, v2);
    mesh.add_face(v_apex1, v4, v3);
    mesh.add_face(v_apex1, v1, v4);

    auto v_apex2 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto u1 = mesh.add_vertex(EK::Point_3(-5, -5, -10));
    auto u2 = mesh.add_vertex(EK::Point_3(5, -5, -10));
    auto u3 = mesh.add_vertex(EK::Point_3(5, 5, -10));
    auto u4 = mesh.add_vertex(EK::Point_3(-5, 5, -10));
    mesh.add_face(u1, u3, u2); mesh.add_face(u1, u4, u3);
    mesh.add_face(v_apex2, u1, u2);
    mesh.add_face(v_apex2, u2, u3);
    mesh.add_face(v_apex2, u3, u4);
    mesh.add_face(v_apex2, u4, u1);
    return mesh;
}

// ============================================================================
// FIXTURE 8: Point-to-Edge (Apex on Edge Midpoint)
// ============================================================================
inline Mesh build_point_to_edge_mesh() {
    Mesh mesh = fix::make_box_mesh(EK::Point_3(-10, -10, -20), EK::Point_3(10, 10, 0));
    auto v_apex = mesh.add_vertex(EK::Point_3(0, 10, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-5, 5, 10));
    auto v2 = mesh.add_vertex(EK::Point_3(5, 5, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(5, 15, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-5, 15, 10));
    mesh.add_face(v1, v2, v3); mesh.add_face(v1, v3, v4);
    mesh.add_face(v_apex, v2, v1);
    mesh.add_face(v_apex, v3, v2);
    mesh.add_face(v_apex, v4, v3);
    mesh.add_face(v_apex, v1, v4);
    return mesh;
}

// ============================================================================
// FIXTURE 9: Point-to-Face (Apex on Face Center)
// ============================================================================
inline Mesh build_point_to_face_mesh() {
    Mesh mesh = fix::make_box_mesh(EK::Point_3(-10, -10, -20), EK::Point_3(10, 10, 0));
    auto v_apex = mesh.add_vertex(EK::Point_3(0, 0, 0));
    auto v1 = mesh.add_vertex(EK::Point_3(-5, -5, 10));
    auto v2 = mesh.add_vertex(EK::Point_3(5, -5, 10));
    auto v3 = mesh.add_vertex(EK::Point_3(5, 5, 10));
    auto v4 = mesh.add_vertex(EK::Point_3(-5, 5, 10));
    mesh.add_face(v1, v2, v3); mesh.add_face(v1, v3, v4);
    mesh.add_face(v_apex, v2, v1);
    mesh.add_face(v_apex, v3, v2);
    mesh.add_face(v_apex, v4, v3);
    mesh.add_face(v_apex, v1, v4);
    return mesh;
}

// ============================================================================
// FIXTURE 10: Edge-to-Face (Prism Knife-Edge on Cube Face)
// ============================================================================
inline Mesh build_edge_to_face_mesh() {
    Mesh mesh = fix::make_box_mesh(EK::Point_3(-10, -10, -20), EK::Point_3(10, 10, 0));
    auto e0 = mesh.add_vertex(EK::Point_3(-5, 0, 0));
    auto e1 = mesh.add_vertex(EK::Point_3(5, 0, 0));
    auto t0 = mesh.add_vertex(EK::Point_3(-5, -5, 10));
    auto t1 = mesh.add_vertex(EK::Point_3(5, -5, 10));
    auto t2 = mesh.add_vertex(EK::Point_3(5, 5, 10));
    auto t3 = mesh.add_vertex(EK::Point_3(-5, 5, 10));
    mesh.add_face(t0, t1, t2); mesh.add_face(t0, t2, t3);
    mesh.add_face(e0, e1, t1); mesh.add_face(e0, t1, t0);
    mesh.add_face(e1, e0, t3); mesh.add_face(e1, t3, t2);
    mesh.add_face(e0, t0, t3);
    mesh.add_face(e1, t2, t1);
    return mesh;
}

// ============================================================================
// FIXTURE 11: Face-to-Face (Two Flush Cubes)
// ============================================================================
inline Mesh build_face_to_face_mesh() {
    Mesh mesh = fix::make_box_mesh(EK::Point_3(-10, -10, -20), EK::Point_3(10, 10, 0));
    Mesh top_cube = fix::make_box_mesh(EK::Point_3(-10, -10, 0), EK::Point_3(10, 10, 20));
    fix::append_mesh(mesh, top_cube);
    return mesh;
}

#include <CGAL/Polygon_mesh_processing/measure.h>

// ============================================================================
// Polyline Prism Generator: Creates watertight 2-manifold triangular prism
// ============================================================================
inline Mesh make_prism_along_polyline(
    const std::vector<EK::Point_3>& pts,
    const EK::Vector_3& offset1,
    const EK::Vector_3& offset2
) {
    Mesh mesh;
    int n = pts.size();
    std::vector<Mesh::Vertex_index> v0(n), v1(n), v2(n);
    for (int i = 0; i < n; ++i) {
        v0[i] = mesh.add_vertex(pts[i]);
        v1[i] = mesh.add_vertex(pts[i] + offset1);
        v2[i] = mesh.add_vertex(pts[i] + offset2);
    }
    // Bottom cap (z_min): outward normal points down
    mesh.add_face(v0[0], v1[0], v2[0]);
    // Top cap (z_max): outward normal points up
    mesh.add_face(v0[n-1], v2[n-1], v1[n-1]);
    // Longitudinal walls with outward-facing right-hand rule winding
    for (int i = 0; i + 1 < n; ++i) {
        mesh.add_face(v0[i], v0[i+1], v1[i+1]); mesh.add_face(v0[i], v1[i+1], v1[i]);
        mesh.add_face(v1[i], v1[i+1], v2[i+1]); mesh.add_face(v1[i], v2[i+1], v2[i]);
        mesh.add_face(v2[i], v2[i+1], v0[i+1]); mesh.add_face(v2[i], v0[i+1], v0[i]);
    }

    // Mathematical validity assertions:
    assert(CGAL::is_closed(mesh));
    assert(CGAL::is_triangle_mesh(mesh));
    assert(!CGAL::Polygon_mesh_processing::does_self_intersect(mesh));
    assert(CGAL::Polygon_mesh_processing::volume(mesh) > 0);

    return mesh;
}

// ============================================================================
// FIXTURE 12: Near-Collinear Polyline (Watertight Manifold Prisms)
// ============================================================================
inline Mesh build_near_collinear_polyline_mesh() {
    std::vector<EK::Point_3> seam = {
        EK::Point_3(0, 0, 0),
        EK::Point_3(0, 0, 4),
        EK::Point_3(EK::FT(1)/EK::FT(1000), 0, 8),
        EK::Point_3(3, 0, 12)
    };
    Mesh left_col = make_prism_along_polyline(seam, EK::Vector_3(-2, -1, 0), EK::Vector_3(-2, 1, 0));
    Mesh right_col = make_prism_along_polyline(seam, EK::Vector_3(2, 1, 0), EK::Vector_3(2, -1, 0));
    fix::append_mesh(left_col, right_col);
    return left_col;
}

// ============================================================================
// FIXTURE 13: Non-Convex L-Shaped Patch with Protected Cavity
// ============================================================================
inline Mesh build_non_convex_L_patch_mesh() {
    Mesh mesh = fix::make_box_mesh(EK::Point_3(-10, -10, -10), EK::Point_3(0, 10, 0));
    Mesh box2 = fix::make_box_mesh(EK::Point_3(0, -10, -10), EK::Point_3(10, 0, 0));
    fix::append_mesh(mesh, box2);

    Mesh pillar = fix::make_box_mesh(EK::Point_3(2, 2, -5), EK::Point_3(8, 8, 5));
    fix::append_mesh(mesh, pillar);

    Mesh top1 = fix::make_box_mesh(EK::Point_3(-10, -10, 0), EK::Point_3(0, 10, 10));
    Mesh top2 = fix::make_box_mesh(EK::Point_3(0, -10, 0), EK::Point_3(10, 0, 10));
    fix::append_mesh(mesh, top1);
    fix::append_mesh(mesh, top2);
    return mesh;
}

} // namespace test
} // namespace geo
} // namespace jotcad
