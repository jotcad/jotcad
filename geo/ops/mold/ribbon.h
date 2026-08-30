#pragma once
#include "types.h"
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/Polygon_mesh_processing/border.h>

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
        
        // Perpendicular lateral normal: r = n - (n * d1) * d1
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
            // Fallback: radial from center axis
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
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& positive_patch_faces,
    const std::vector<std::pair<int, int>>& boundary_cycle,
    const EK::Vector_3& d1,
    const EK::Point_3& center,
    FT mx_min, FT mx_max,
    FT my_min, FT my_max,
    FT mz_min, FT mz_max
) {
    ExactMesh mesh_solid_wedge;

    // 1. Vertex Normals in pure FT
    std::vector<EK::Vector_3> v_normals(mesh_part.number_of_vertices(), EK::Vector_3(FT(0), FT(0), FT(0)));
    for (auto f : mesh_part.faces()) {
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        EK::Vector_3 fn = CGAL::normal(p0, p1, p2);
        v_normals[(size_t)mesh_part.source(h)] = v_normals[(size_t)mesh_part.source(h)] + fn;
        v_normals[(size_t)mesh_part.target(h)] = v_normals[(size_t)mesh_part.target(h)] + fn;
        v_normals[(size_t)mesh_part.target(mesh_part.next(h))] = v_normals[(size_t)mesh_part.target(mesh_part.next(h))] + fn;
    }

    FT hx = (mx_max - mx_min) / FT(2);
    FT hy = (my_max - my_min) / FT(2);
    FT hz = (mz_max - mz_min) / FT(2);
    FT r_box_sq = hx*hx + hy*hy + hz*hz;
    double r_box = std::sqrt(CGAL::to_double(r_box_sq));
    FT r_envelope = FT(r_box * 2.0 + 50.0);

    std::map<int, ExactMesh::Vertex_index> v_in_map;
    std::map<int, ExactMesh::Vertex_index> v_out_map;
    std::map<int, ExactMesh::Vertex_index> v_top_outer_map;

    auto get_in_v = [&](int old_idx) -> ExactMesh::Vertex_index {
        auto it = v_in_map.find(old_idx);
        if (it != v_in_map.end()) return it->second;
        auto p = mesh_part.point(ExactMesh::Vertex_index(old_idx));
        auto v = mesh_solid_wedge.add_vertex(p);
        v_in_map[old_idx] = v;
        return v;
    };

    auto get_out_v = [&](int old_idx) -> ExactMesh::Vertex_index {
        auto it = v_out_map.find(old_idx);
        if (it != v_out_map.end()) return it->second;
        
        EK::Vector_3 vn = v_normals[old_idx];
        FT dot = vn * d1;
        EK::Vector_3 lateral_r = vn - d1 * dot;

        double r_len = std::sqrt(CGAL::to_double(lateral_r.squared_length()));
        auto in_p = mesh_part.point(ExactMesh::Vertex_index(old_idx));
        EK::Point_3 out_p;
        if (r_len > 1e-9) {
            FT scale = r_envelope / FT(r_len);
            out_p = EK::Point_3(in_p.x() + lateral_r.x() * scale, in_p.y() + lateral_r.y() * scale, in_p.z() + lateral_r.z() * scale);
        } else {
            EK::Vector_3 radial_r(in_p.x() - center.x(), in_p.y() - center.y(), in_p.z() - center.z());
            radial_r = radial_r - d1 * (radial_r * d1);
            double rad_len = std::sqrt(CGAL::to_double(radial_r.squared_length()));
            FT scale = r_envelope / FT(std::max(rad_len, 1e-6));
            out_p = EK::Point_3(in_p.x() + radial_r.x() * scale, in_p.y() + radial_r.y() * scale, in_p.z() + radial_r.z() * scale);
        }

        auto v = mesh_solid_wedge.add_vertex(out_p);
        v_out_map[old_idx] = v;
        return v;
    };

    auto get_top_out_v = [&](int old_idx) -> ExactMesh::Vertex_index {
        auto it = v_top_outer_map.find(old_idx);
        if (it != v_top_outer_map.end()) return it->second;
        auto out_p = mesh_solid_wedge.point(get_out_v(old_idx));
        EK::Point_3 top_p(out_p.x() + d1.x() * r_envelope, out_p.y() + d1.y() * r_envelope, out_p.z() + d1.z() * r_envelope);
        auto v = mesh_solid_wedge.add_vertex(top_p);
        v_top_outer_map[old_idx] = v;
        return v;
    };

    // 1. Parting Ribbon Bottom Floor Quads: directly on boundary_cycle
    for (const auto& [u, v] : boundary_cycle) {
        auto u_in = get_in_v(u);
        auto v_in = get_in_v(v);
        auto u_out = get_out_v(u);
        auto v_out = get_out_v(v);

        mesh_solid_wedge.add_face(u_in, v_in, v_out);
        mesh_solid_wedge.add_face(u_in, v_out, u_out);
    }

    // 2. Reversed Positive Model Skin Patch S_1^rev
    for (auto f : positive_patch_faces) {
        auto h = mesh_part.halfedge(f);
        int i0 = (int)mesh_part.source(h);
        int i1 = (int)mesh_part.target(h);
        int i2 = (int)mesh_part.target(mesh_part.next(h));

        auto v0 = get_in_v(i0);
        auto v1 = get_in_v(i1);
        auto v2 = get_in_v(i2);
        mesh_solid_wedge.add_face(v0, v2, v1);
    }

    // 3. Outer Wall Quads
    for (const auto& [u, v] : boundary_cycle) {
        auto u_out = get_out_v(u);
        auto v_out = get_out_v(v);
        auto u_top_out = get_top_out_v(u);
        auto v_top_out = get_top_out_v(v);

        mesh_solid_wedge.add_face(u_out, v_out, v_top_out);
        mesh_solid_wedge.add_face(u_out, v_top_out, u_top_out);
    }

    // 4. Seal top planar ceiling using Centroid Fan on boundary_cycle
    EK::Point_3 top_center(center.x() + d1.x() * (r_envelope + FT(10)),
                           center.y() + d1.y() * (r_envelope + FT(10)),
                           center.z() + d1.z() * (r_envelope + FT(10)));
    auto c_top = mesh_solid_wedge.add_vertex(top_center);
    for (const auto& [u, v] : boundary_cycle) {
        auto u_top = get_top_out_v(u);
        auto v_top = get_top_out_v(v);
        mesh_solid_wedge.add_face(u_top, v_top, c_top);
    }

    CGAL::Polygon_mesh_processing::stitch_borders(mesh_solid_wedge);
    mesh_solid_wedge.collect_garbage();

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
