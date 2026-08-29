#pragma once
#include "types.h"
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

namespace jotcad {
namespace geo {
namespace mold {

inline Geometry build_parting_ribbon_surface(
    const Geometry& world_geo,
    const std::vector<std::pair<int, int>>& best_parting_segments,
    const EK::Vector_3& d1,
    const EK::Point_3& center,
    FT mx_min, FT mx_max,
    FT my_min, FT my_max,
    FT mz_min, FT mz_max
) {
    // 1. Compute vertex normals in pure FT
    std::vector<EK::Vector_3> v_normals(world_geo.vertices.size(), EK::Vector_3(FT(0), FT(0), FT(0)));
    for (const auto& tri : world_geo.triangles) {
        const auto& v0 = world_geo.vertices[tri[0]];
        const auto& v1 = world_geo.vertices[tri[1]];
        const auto& v2 = world_geo.vertices[tri[2]];
        EK::Point_3 p0(v0.x, v0.y, v0.z), p1(v1.x, v1.y, v1.z), p2(v2.x, v2.y, v2.z);
        EK::Vector_3 fn = CGAL::normal(p0, p1, p2);
        v_normals[tri[0]] = v_normals[tri[0]] + fn;
        v_normals[tri[1]] = v_normals[tri[1]] + fn;
        v_normals[tri[2]] = v_normals[tri[2]] + fn;
    }

    FT hx = (mx_max - mx_min) / FT(2);
    FT hy = (my_max - my_min) / FT(2);
    FT hz = (mz_max - mz_min) / FT(2);
    FT r_box_sq = hx*hx + hy*hy + hz*hz;
    double r_box = std::sqrt(CGAL::to_double(r_box_sq));
    FT r_envelope = FT(r_box * 2.0 + 50.0);

    Geometry parting_ribbon_geo;
    std::map<int, int> v_inner_map;
    std::map<int, int> v_outer_map;

    auto get_inner_v = [&](int old_idx) -> int {
        auto it = v_inner_map.find(old_idx);
        if (it != v_inner_map.end()) return it->second;
        int n_idx = (int)parting_ribbon_geo.vertices.size();
        parting_ribbon_geo.vertices.push_back(world_geo.vertices[old_idx]);
        v_inner_map[old_idx] = n_idx;
        return n_idx;
    };

    auto get_outer_v = [&](int old_idx) -> int {
        auto it = v_outer_map.find(old_idx);
        if (it != v_outer_map.end()) return it->second;
        int n_idx = (int)parting_ribbon_geo.vertices.size();
        
        EK::Vector_3 vn = v_normals[old_idx];
        FT dot = vn * d1;
        EK::Vector_3 lateral_r = vn - d1 * dot;

        double r_len = std::sqrt(CGAL::to_double(lateral_r.squared_length()));
        const auto& in_v = world_geo.vertices[old_idx];
        Vertex out_v;
        if (r_len > 1e-9) {
            FT scale = r_envelope / FT(r_len);
            out_v.x = in_v.x + lateral_r.x() * scale;
            out_v.y = in_v.y + lateral_r.y() * scale;
            out_v.z = in_v.z + lateral_r.z() * scale;
        } else {
            EK::Vector_3 radial_r(in_v.x - center.x(), in_v.y - center.y(), in_v.z - center.z());
            radial_r = radial_r - d1 * (radial_r * d1);
            double rad_len = std::sqrt(CGAL::to_double(radial_r.squared_length()));
            FT scale = r_envelope / FT(std::max(rad_len, 1e-6));
            out_v.x = in_v.x + radial_r.x() * scale;
            out_v.y = in_v.y + radial_r.y() * scale;
            out_v.z = in_v.z + radial_r.z() * scale;
        }

        parting_ribbon_geo.vertices.push_back(out_v);
        v_outer_map[old_idx] = n_idx;
        return n_idx;
    };

    for (const auto& seg : best_parting_segments) {
        int u_in = get_inner_v(seg.first);
        int v_in = get_inner_v(seg.second);
        int u_out = get_outer_v(seg.first);
        int v_out = get_outer_v(seg.second);

        parting_ribbon_geo.triangles.push_back({u_in, v_in, v_out});
        parting_ribbon_geo.triangles.push_back({u_in, v_out, u_out});
    }

    return parting_ribbon_geo;
}

inline ExactMesh build_parting_solid_wedge(
    const ExactMesh& envelope_patch_mesh,
    const EK::Vector_3& d1,
    const EK::Point_3& center,
    FT mx_min, FT mx_max,
    FT my_min, FT my_max,
    FT mz_min, FT mz_max
) {
    FT hx = (mx_max - mx_min) / FT(2);
    FT hy = (my_max - my_min) / FT(2);
    FT hz = (mz_max - mz_min) / FT(2);
    FT r_box_sq = hx*hx + hy*hy + hz*hz;
    double r_box = std::sqrt(CGAL::to_double(r_box_sq));
    FT r_envelope = FT(r_box * 2.0 + 50.0);

    std::vector<EK::Point_3> soup_points;
    std::vector<std::vector<size_t>> soup_polygons;

    std::map<int, size_t> v_in_map;
    std::map<int, size_t> v_top_map;

    auto get_in_v = [&](int old_idx) -> size_t {
        auto it = v_in_map.find(old_idx);
        if (it != v_in_map.end()) return it->second;
        auto p = envelope_patch_mesh.point(ExactMesh::Vertex_index(old_idx));
        size_t idx = soup_points.size();
        soup_points.push_back(p);
        v_in_map[old_idx] = idx;
        return idx;
    };

    FT d1_sq = d1.x()*d1.x() + d1.y()*d1.y() + d1.z()*d1.z();
    FT h_max = -1000000;
    for (auto v : envelope_patch_mesh.vertices()) {
        auto p = envelope_patch_mesh.point(v);
        FT d_val = (p.x()*d1.x() + p.y()*d1.y() + p.z()*d1.z()) / d1_sq;
        if (d_val > h_max) h_max = d_val;
    }
    FT h_ceiling = h_max + r_envelope;

    auto get_top_v = [&](int old_idx) -> size_t {
        auto it = v_top_map.find(old_idx);
        if (it != v_top_map.end()) return it->second;
        auto in_p = envelope_patch_mesh.point(ExactMesh::Vertex_index(old_idx));
        FT in_h = (in_p.x()*d1.x() + in_p.y()*d1.y() + in_p.z()*d1.z()) / d1_sq;
        FT t = h_ceiling - in_h;
        EK::Point_3 top_p(
            in_p.x() + d1.x() * t,
            in_p.y() + d1.y() * t,
            in_p.z() + d1.z() * t
        );
        size_t idx = soup_points.size();
        soup_points.push_back(top_p);
        v_top_map[old_idx] = idx;
        return idx;
    };

    // 1. Reversed Positive Model Cavity Skin Patch S_1^rev
    for (auto f : envelope_patch_mesh.faces()) {
        auto h = envelope_patch_mesh.halfedge(f);
        int i0 = (int)envelope_patch_mesh.source(h);
        int i1 = (int)envelope_patch_mesh.target(h);
        int i2 = (int)envelope_patch_mesh.target(envelope_patch_mesh.next(h));

        auto v0 = get_in_v(i0);
        auto v1 = get_in_v(i1);
        auto v2 = get_in_v(i2);
        soup_polygons.push_back({v0, v2, v1});
    }

    // 2. Swept Sidewall Quads along uniform draw direction d1
    for (auto h : envelope_patch_mesh.halfedges()) {
        if (envelope_patch_mesh.is_border(h)) {
            int u = (int)envelope_patch_mesh.source(h);
            int v = (int)envelope_patch_mesh.target(h);
            auto u_in = get_in_v(u);
            auto v_in = get_in_v(v);
            auto u_top = get_top_v(u);
            auto v_top = get_top_v(v);

            soup_polygons.push_back({u_in, v_in, v_top});
            soup_polygons.push_back({v_top, u_top, u_in});
        }
    }

    // 3. Global polygon soup orientation and mesh synthesis
    CGAL::Polygon_mesh_processing::repair_polygon_soup(soup_points, soup_polygons);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(soup_points, soup_polygons);
    ExactMesh mesh_solid_wedge;
    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(soup_points, soup_polygons, mesh_solid_wedge);
    CGAL::Polygon_mesh_processing::stitch_borders(mesh_solid_wedge);

    // 4. Seal Top Ceiling via native CGAL Hole Triangulation
    std::vector<ExactMesh::Halfedge_index> border_halfedges;
    CGAL::Polygon_mesh_processing::extract_boundary_cycles(mesh_solid_wedge, std::back_inserter(border_halfedges));
    for (auto h_border : border_halfedges) {
        std::vector<ExactMesh::Face_index> patch_facets;
        CGAL::Polygon_mesh_processing::triangulate_hole(mesh_solid_wedge, h_border, std::back_inserter(patch_facets));
    }

    CGAL::Polygon_mesh_processing::stitch_borders(mesh_solid_wedge);
    mesh_solid_wedge.collect_garbage();

    if (CGAL::is_closed(mesh_solid_wedge)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(mesh_solid_wedge);
    }

    bool is_closed = CGAL::is_closed(mesh_solid_wedge);
    bool self_intersects = CGAL::Polygon_mesh_processing::does_self_intersect(mesh_solid_wedge);
    std::cout << "  [Wedge Validation] is_closed: " << (is_closed ? "YES" : "NO")
              << " | does_self_intersect: " << (self_intersects ? "YES" : "NO")
              << " | vertices: " << mesh_solid_wedge.number_of_vertices()
              << " | faces: " << mesh_solid_wedge.number_of_faces() << std::endl;

    return mesh_solid_wedge;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
