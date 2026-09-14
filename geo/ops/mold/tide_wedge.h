#pragma once
#include "types.h"
#include "tide.h"
#include <vector>
#include <set>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Generates an exact 2-manifold closed solid mold piece using Rising Tide shelf and projection skirt.
 */
inline EnvelopeWedgeResult construct_rising_tide_wedge(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& patch_faces,
    const EK::Vector_3& draw_dir,
    const FT& stock_half_extent,
    const FT& shelf_margin_h = FT(0)
) {
    if (patch_faces.empty()) return {};

    TideBasis basis = compute_cardinal_tide_basis(draw_dir);
    FT denom = -(basis.normal * draw_dir);
    if (denom == FT(0)) denom = FT(-1); // Safety fallback

    auto dot_basis = [&](const EK::Point_3& p, const EK::Vector_3& axis) -> FT {
        return p.x() * axis.x() + p.y() * axis.y() + p.z() * axis.z();
    };

    auto project_to_shelf = [&](const EK::Point_3& p) -> EK::Point_3 {
        FT h_p = dot_basis(p, basis.normal);
        FT t = (shelf_margin_h - h_p) / denom;
        return EK::Point_3(
            p.x() - t * draw_dir.x(),
            p.y() - t * draw_dir.y(),
            p.z() - t * draw_dir.z()
        );
    };

    auto to_shelf_uv = [&](const EK::Point_3& p) -> CDT_Kernel::Point_2 {
        return CDT_Kernel::Point_2(dot_basis(p, basis.u), dot_basis(p, basis.v));
    };

    auto from_shelf_uv = [&](const FT& u, const FT& v, const FT& h) -> EK::Point_3 {
        return EK::Point_3(
            u * basis.u.x() + v * basis.v.x() + h * basis.normal.x(),
            u * basis.u.y() + v * basis.v.y() + h * basis.normal.y(),
            u * basis.u.z() + v * basis.v.z() + h * basis.normal.z()
        );
    };

    std::vector<EK::Point_3> soup_points;
    std::vector<std::vector<size_t>> soup_polygons;
    std::set<size_t> source_faces;

    // 1. Cavity Floor: Direct patch triangles from mesh_part
    for (auto f : patch_faces) {
        source_faces.insert(f.idx());
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));

        size_t idx0 = soup_points.size();
        soup_points.push_back(p0);
        soup_points.push_back(p1);
        soup_points.push_back(p2);
        // Reverse winding for cavity impression (outward from mold piece)
        soup_polygons.push_back({idx0, idx0 + 2, idx0 + 1});
    }

    // 2. Projection Skirt: Extruded from 3D patch border loops to shelf plane
    auto border_loops = extract_patch_border_loops(mesh_part, patch_faces);
    std::vector<std::vector<CDT_Kernel::Point_2>> projected_2d_loops;

    for (const auto& loop : border_loops) {
        if (loop.size() < 3) continue;
        std::vector<CDT_Kernel::Point_2> loop_2d;
        loop_2d.reserve(loop.size());

        for (size_t i = 0; i < loop.size(); ++i) {
            size_t j = (i + 1) % loop.size();
            const auto& pa = loop[i];
            const auto& pb = loop[j];
            auto pa_proj = project_to_shelf(pa);
            auto pb_proj = project_to_shelf(pb);

            loop_2d.push_back(to_shelf_uv(pa_proj));

            size_t s_idx = soup_points.size();
            soup_points.push_back(pa);
            soup_points.push_back(pb);
            soup_points.push_back(pb_proj);
            soup_points.push_back(pa_proj);

            // Skirt quad triangulated outward
            soup_polygons.push_back({s_idx, s_idx + 1, s_idx + 2});
            soup_polygons.push_back({s_idx, s_idx + 2, s_idx + 3});
        }
        projected_2d_loops.push_back(std::move(loop_2d));
    }

    // 3. Planar Margin Shelf: 2D CDT between projected loops and stock boundary at shelf_margin_h
    FT u_min = -stock_half_extent;
    FT u_max = stock_half_extent;
    FT v_min = -stock_half_extent;
    FT v_max = stock_half_extent;
    FT h_top = stock_half_extent;

    ExactCDT cdt;
    auto vh_c0 = cdt.insert(CDT_Kernel::Point_2(u_min, v_min));
    auto vh_c1 = cdt.insert(CDT_Kernel::Point_2(u_max, v_min));
    auto vh_c2 = cdt.insert(CDT_Kernel::Point_2(u_max, v_max));
    auto vh_c3 = cdt.insert(CDT_Kernel::Point_2(u_min, v_max));

    cdt.insert_constraint(vh_c0, vh_c1);
    cdt.insert_constraint(vh_c1, vh_c2);
    cdt.insert_constraint(vh_c2, vh_c3);
    cdt.insert_constraint(vh_c3, vh_c0);

    for (const auto& loop_2d : projected_2d_loops) {
        std::vector<ExactCDT::Vertex_handle> hole_vhs;
        hole_vhs.reserve(loop_2d.size());
        for (const auto& pt2d : loop_2d) {
            hole_vhs.push_back(cdt.insert(pt2d));
        }
        for (size_t i = 0; i < hole_vhs.size(); ++i) {
            cdt.insert_constraint(hole_vhs[i], hole_vhs[(i + 1) % hole_vhs.size()]);
        }
    }

    CGAL::mark_domain_in_triangulation(cdt);

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->info().in_domain) continue;

        auto p0_2d = fit->vertex(0)->point();
        auto p1_2d = fit->vertex(1)->point();
        auto p2_2d = fit->vertex(2)->point();

        auto p0_3d = from_shelf_uv(p0_2d.x(), p0_2d.y(), shelf_margin_h);
        auto p1_3d = from_shelf_uv(p1_2d.x(), p1_2d.y(), shelf_margin_h);
        auto p2_3d = from_shelf_uv(p2_2d.x(), p2_2d.y(), shelf_margin_h);

        size_t s_idx = soup_points.size();
        soup_points.push_back(p0_3d);
        soup_points.push_back(p2_3d);
        soup_points.push_back(p1_3d);
        soup_polygons.push_back({s_idx, s_idx + 1, s_idx + 2});
    }

    // 4. Stock Box Side Walls and Ceiling
    auto add_quad = [&](const EK::Point_3& a, const EK::Point_3& b, const EK::Point_3& c, const EK::Point_3& d) {
        size_t idx = soup_points.size();
        soup_points.push_back(a);
        soup_points.push_back(b);
        soup_points.push_back(c);
        soup_points.push_back(d);
        soup_polygons.push_back({idx, idx + 1, idx + 2});
        soup_polygons.push_back({idx, idx + 2, idx + 3});
    };

    auto b0 = from_shelf_uv(u_min, v_min, shelf_margin_h);
    auto b1 = from_shelf_uv(u_max, v_min, shelf_margin_h);
    auto b2 = from_shelf_uv(u_max, v_max, shelf_margin_h);
    auto b3 = from_shelf_uv(u_min, v_max, shelf_margin_h);

    auto t0 = from_shelf_uv(u_min, v_min, h_top);
    auto t1 = from_shelf_uv(u_max, v_min, h_top);
    auto t2 = from_shelf_uv(u_max, v_max, h_top);
    auto t3 = from_shelf_uv(u_min, v_max, h_top);

    // 4 outer side walls
    add_quad(b0, b1, t1, t0);
    add_quad(b1, b2, t2, t1);
    add_quad(b2, b3, t3, t2);
    add_quad(b3, b0, t0, t3);

    // Ceiling
    add_quad(t0, t1, t2, t3);

    // Regularize, orient, and assert manifold solid
    fix::repair_solid_soup(soup_points, soup_polygons);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(soup_points, soup_polygons);

    ExactMesh piece_mesh;
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(soup_points, soup_polygons, piece_mesh);
    CGAL::Polygon_mesh_processing::stitch_borders(piece_mesh);
    CGAL::Polygon_mesh_processing::triangulate_faces(piece_mesh);
    piece_mesh.collect_garbage();

    if (CGAL::is_closed(piece_mesh)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(piece_mesh);
    }
    fix::assert_well_formed_closed_mesh(piece_mesh, "construct_rising_tide_wedge");

    return {piece_mesh, source_faces};
}

} // namespace mold
} // namespace geo
} // namespace jotcad
