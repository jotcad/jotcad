#pragma once
#include "types.h"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/Env_triangle_traits_3.h>
#include <CGAL/Env_surface_data_traits_3.h>
#include <CGAL/envelope_3.h>
#include <vector>
#include <set>
#include <map>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

typedef CGAL::Env_triangle_traits_3<EK> Env_traits;
typedef CGAL::Env_surface_data_traits_3<Env_traits, size_t> Data_traits;
typedef Data_traits::Surface_3 Data_triangle_3;
typedef CGAL::Envelope_diagram_2<Data_traits> Envelope_diagram_2;

// Triangulates a vertical wall quad monotonically between height lists along halfedge h
inline void add_monotonic_vertical_wall(
    Envelope_diagram_2::Halfedge_handle h,
    FT low_s, FT low_t, FT high_s, FT high_t,
    std::map<Envelope_diagram_2::Vertex_handle, std::set<FT>>& vertex_heights,
    std::vector<EK::Point_3>& soup_points,
    std::vector<std::vector<size_t>>& soup_polygons
) {
    if (low_s == high_s && low_t == high_t) return;
    auto p1_2d = h->source()->point();
    auto p2_2d = h->target()->point();

    std::vector<FT> left_zs;
    for (FT z : vertex_heights[h->source()]) {
        if (z >= low_s && z <= high_s) left_zs.push_back(z);
    }
    std::sort(left_zs.begin(), left_zs.end());
    if (left_zs.empty() || left_zs.front() > low_s) left_zs.insert(left_zs.begin(), low_s);
    if (left_zs.back() < high_s) left_zs.push_back(high_s);

    std::vector<FT> right_zs;
    for (FT z : vertex_heights[h->target()]) {
        if (z >= low_t && z <= high_t) right_zs.push_back(z);
    }
    std::sort(right_zs.begin(), right_zs.end());
    if (right_zs.empty() || right_zs.front() > low_t) right_zs.insert(right_zs.begin(), low_t);
    if (right_zs.back() < high_t) right_zs.push_back(high_t);

    size_t i = 0, j = 0;
    size_t N = left_zs.size(), M = right_zs.size();
    FT span_L = high_s - low_s;
    FT span_R = high_t - low_t;

    while (i + 1 < N || j + 1 < M) {
        bool advance_left = false;
        if (i + 1 < N && j + 1 < M) {
            FT t_L = (span_L > FT(0)) ? (left_zs[i + 1] - low_s) / span_L : FT(1);
            FT t_R = (span_R > FT(0)) ? (right_zs[j + 1] - low_t) / span_R : FT(1);
            advance_left = (t_L <= t_R);
        } else if (i + 1 < N) {
            advance_left = true;
        } else {
            advance_left = false;
        }

        if (advance_left) {
            size_t idx = soup_points.size();
            soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j]));
            soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i]));
            soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i + 1]));
            soup_polygons.push_back({idx, idx + 1, idx + 2});
            i++;
        } else {
            size_t idx = soup_points.size();
            soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j]));
            soup_points.push_back(EK::Point_3(p1_2d.x(), p1_2d.y(), left_zs[i]));
            soup_points.push_back(EK::Point_3(p2_2d.x(), p2_2d.y(), right_zs[j + 1]));
            soup_polygons.push_back({idx, idx + 1, idx + 2});
            j++;
        }
    }
}

/**
 * @brief Precomputes consistent base elevations for outer boundary vertices.
 * 
 * 1. For internal cliff terminals meeting the perimeter, drops to minimum incident surface height.
 * 2. For outer boundary edges with vertical model faces below them, drops to minimum vertical face height.
 */
inline std::map<Envelope_diagram_2::Vertex_handle, FT> compute_outer_boundary_base_heights(
    Envelope_diagram_2& max_diag,
    const std::function<FT(size_t, const FT&, const FT&)>& get_z,
    const std::function<std::optional<FT>(const CDT_Kernel::Point_2&, const CDT_Kernel::Point_2&)>& get_vertical_drop
) {
    std::map<Envelope_diagram_2::Vertex_handle, FT> vertex_base_z;

    // 1. Any vertex where multiple envelope surfaces meet with different heights is an internal cliff terminal
    for (auto vit = max_diag.vertices_begin(); vit != max_diag.vertices_end(); ++vit) {
        std::set<FT> s_zs;
        auto e_curr = vit->incident_halfedges();
        auto e_start = e_curr;
        do {
            auto f = e_curr->face();
            if (!f->is_unbounded() && f->number_of_surfaces() > 0) {
                size_t orig_f = f->surfaces_begin()->data();
                s_zs.insert(get_z(orig_f, vit->point().x(), vit->point().y()));
            }
            ++e_curr;
        } while (e_curr != e_start);

        if (s_zs.size() > 1) {
            // Drop to minimum surface height among incident faces to close cliff seam
            vertex_base_z[vit] = *s_zs.begin();
        }
    }

    // 2. Any outer boundary edge with vertical model faces below it sets its endpoints to v_min
    for (auto fit = max_diag.faces_begin(); fit != max_diag.faces_end(); ++fit) {
        if (fit->is_unbounded() || fit->number_of_surfaces() == 0) continue;

        auto check_edge = [&](Envelope_diagram_2::Halfedge_handle h) {
            auto twin_face = h->twin()->face();
            if (twin_face->is_unbounded() || twin_face->number_of_surfaces() == 0) {
                auto p1_2d = h->source()->point();
                auto p2_2d = h->target()->point();
                if (get_vertical_drop) {
                    auto v_drop = get_vertical_drop(p1_2d, p2_2d);
                    if (v_drop.has_value()) {
                        FT v_min = *v_drop;
                        auto it1 = vertex_base_z.find(h->source());
                        if (it1 == vertex_base_z.end() || v_min < it1->second) {
                            vertex_base_z[h->source()] = v_min;
                        }
                        auto it2 = vertex_base_z.find(h->target());
                        if (it2 == vertex_base_z.end() || v_min < it2->second) {
                            vertex_base_z[h->target()] = v_min;
                        }
                    }
                }
            }
        };

        auto ccb = fit->outer_ccb();
        auto curr = ccb;
        do { check_edge(curr); curr = curr->next(); } while (curr != ccb);
        for (auto hole_it = fit->holes_begin(); hole_it != fit->holes_end(); ++hole_it) {
            auto h_curr = *hole_it;
            auto h_start = h_curr;
            do { check_edge(h_curr); h_curr = h_curr->next(); } while (h_curr != h_start);
        }
    }

    return vertex_base_z;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
