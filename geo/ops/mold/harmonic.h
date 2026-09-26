#pragma once

#include "types.h"
#include "fix/assert_mesh.h"
#include "fix/soup_repair.h"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <vector>
#include <map>
#include <set>
#include <functional>
#include <iostream>
#include <algorithm>

namespace jotcad {
namespace geo {
namespace mold {

struct HarmonicStockParams {
    FT u_min = FT(0), u_max = FT(0);
    FT v_min = FT(0), v_max = FT(0);
    FT w_top = FT(0);
};

struct HarmonicVertexInfo {
    EK::FT w = EK::FT(0);
    bool is_fixed = false;
    int index = -1;
};

template <typename Gt, typename Vb_base = CGAL::Triangulation_vertex_base_2<Gt>>
class Harmonic_Vertex_with_info : public Vb_base {
    HarmonicVertexInfo _info;
public:
    typedef Gt Geom_traits;
    typedef typename Vb_base::Point Point;
    typedef typename Vb_base::Face_handle Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Vb_base::template Rebind_TDS<TDS2>::Other Vb2;
        typedef Harmonic_Vertex_with_info<Gt, Vb2> Other;
    };

    Harmonic_Vertex_with_info() : Vb_base() {}
    Harmonic_Vertex_with_info(const Point& p) : Vb_base(p) {}
    Harmonic_Vertex_with_info(const Point& p, Face_handle f) : Vb_base(p, f) {}
    Harmonic_Vertex_with_info(Face_handle f) : Vb_base(f) {}
    HarmonicVertexInfo& info() { return _info; }
    const HarmonicVertexInfo& info() const { return _info; }
};

typedef Harmonic_Vertex_with_info<CDT_Kernel> Harmonic_Vb;
typedef CDT_Face_with_info<CDT_Kernel> Harmonic_Fb;
typedef CGAL::Triangulation_data_structure_2<Harmonic_Vb, Harmonic_Fb> Harmonic_TDS;
typedef CGAL::Constrained_Delaunay_triangulation_2<CDT_Kernel, Harmonic_TDS, CDT_Itag> HarmonicCDT;

/**
 * @brief Synthesizes an area-minimizing harmonic parting surface over the 2D annular CDT domain.
 * 
 * Solves the discrete Dirichlet energy / Laplace equation Delta w = 0 with natural Neumann
 * boundary condition on stock walls (orthogonal flush exit) and obstacle clearance constraints.
 */
inline void solve_harmonic_parting_surface(
    const std::vector<BoundarySegment3D>& inner_segments,
    const HarmonicStockParams& stock,
    std::vector<EK::Point_3>& soup_points,
    std::vector<std::vector<size_t>>& soup_polygons,
    const std::function<std::optional<FT>(const CDT_Kernel::Point_2&)>& obstacle_fn = nullptr,
    const std::vector<BoundarySegment3D>& prior_seams = {},
    int max_iterations = 60
) {
    HarmonicCDT cdt;
    std::map<CDT_Kernel::Point_2, HarmonicCDT::Vertex_handle> v_map;

    auto get_or_insert = [&](const CDT_Kernel::Point_2& pt, const FT& init_w, bool fixed) {
        auto it = v_map.find(pt);
        if (it != v_map.end()) {
            if (fixed) {
                it->second->info().is_fixed = true;
                it->second->info().w = init_w;
            }
            return it->second;
        }
        auto vh = cdt.insert(pt);
        vh->info().w = init_w;
        vh->info().is_fixed = fixed;
        v_map[pt] = vh;
        return vh;
    };

    // 1. Insert outer stock boundary frame
    auto vh_c0 = get_or_insert(CDT_Kernel::Point_2(stock.u_min, stock.v_min), FT(0), false);
    auto vh_c1 = get_or_insert(CDT_Kernel::Point_2(stock.u_max, stock.v_min), FT(0), false);
    auto vh_c2 = get_or_insert(CDT_Kernel::Point_2(stock.u_max, stock.v_max), FT(0), false);
    auto vh_c3 = get_or_insert(CDT_Kernel::Point_2(stock.u_min, stock.v_max), FT(0), false);

    cdt.insert_constraint(vh_c0, vh_c1);
    cdt.insert_constraint(vh_c1, vh_c2);
    cdt.insert_constraint(vh_c2, vh_c3);
    cdt.insert_constraint(vh_c3, vh_c0);

    // 2. Insert inner model patch boundary obligations (fixed Dirichlet boundary)
    FT sum_fixed_w = 0;
    size_t count_fixed_w = 0;
    for (const auto& seg : inner_segments) {
        if (seg.p1_2d != seg.p2_2d) {
            auto vh_a = get_or_insert(seg.p1_2d, seg.z1, true);
            auto vh_b = get_or_insert(seg.p2_2d, seg.z2, true);
            cdt.insert_constraint(vh_a, vh_b);
            sum_fixed_w += seg.z1 + seg.z2;
            count_fixed_w += 2;
        }
    }

    // 3. Insert prior piece parting seams (fixed Dirichlet boundary)
    for (const auto& seg : prior_seams) {
        if (seg.p1_2d != seg.p2_2d) {
            auto vh_a = get_or_insert(seg.p1_2d, seg.z1, true);
            auto vh_b = get_or_insert(seg.p2_2d, seg.z2, true);
            cdt.insert_constraint(vh_a, vh_b);
            sum_fixed_w += seg.z1 + seg.z2;
            count_fixed_w += 2;
        }
    }

    FT avg_fixed_w = (count_fixed_w > 0) ? (sum_fixed_w / FT(count_fixed_w)) : FT(0);

    // 4. Mark domain in triangulation (annular region between inner patch and outer stock)
    CGAL::mark_domain_in_triangulation(cdt);

    // 5. Collect all active vertices belonging to in_domain faces
    std::vector<HarmonicCDT::Vertex_handle> active_vertices;
    std::set<HarmonicCDT::Vertex_handle> active_set;

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->info().in_domain) continue;
        for (int i = 0; i < 3; ++i) {
            auto v = fit->vertex(i);
            if (active_set.insert(v).second) {
                v->info().index = (int)active_vertices.size();
                if (!v->info().is_fixed) {
                    v->info().w = avg_fixed_w;
                }
                active_vertices.push_back(v);
            }
        }
    }

    if (active_vertices.empty()) return;

    // 6. Build adjacency graph with exact rational inverse-distance-squared weights
    size_t V = active_vertices.size();
    std::vector<std::vector<std::pair<size_t, FT>>> adj(V);
    std::set<std::pair<size_t, size_t>> seen_edges;

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->info().in_domain) continue;
        for (int i = 0; i < 3; ++i) {
            auto v1 = fit->vertex(i);
            auto v2 = fit->vertex((i + 1) % 3);
            size_t idx1 = (size_t)v1->info().index;
            size_t idx2 = (size_t)v2->info().index;
            if (idx1 > idx2) std::swap(idx1, idx2);
            if (seen_edges.insert({idx1, idx2}).second) {
                FT d2 = (v1->point() - v2->point()).squared_length();
                if (d2 > FT(0)) {
                    FT weight = FT(1) / d2;
                    adj[idx1].push_back({idx2, weight});
                    adj[idx2].push_back({idx1, weight});
                }
            }
        }
    }

    // 7. Solve discrete Laplace equation via Projected Gauss-Seidel
    for (int iter = 0; iter < max_iterations; ++iter) {
        for (size_t i = 0; i < V; ++i) {
            if (active_vertices[i]->info().is_fixed) continue;
            FT sum_w = FT(0);
            FT sum_wv = FT(0);
            for (const auto& [j, w_ij] : adj[i]) {
                sum_w += w_ij;
                sum_wv += w_ij * active_vertices[j]->info().w;
            }
            if (sum_w > FT(0)) {
                FT new_w = sum_wv / sum_w;
                if (obstacle_fn) {
                    auto obs = obstacle_fn(active_vertices[i]->point());
                    if (obs && *obs > new_w) new_w = *obs;
                }
                active_vertices[i]->info().w = new_w;
            }
        }
    }

    // 8. Output parting triangles to soup (oriented with outward normal along +Z)
    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->info().in_domain) continue;

        auto v0 = fit->vertex(0);
        auto v1 = fit->vertex(1);
        auto v2 = fit->vertex(2);

        EK::Point_3 p0(v0->point().x(), v0->point().y(), v0->info().w);
        EK::Point_3 p1(v1->point().x(), v1->point().y(), v1->info().w);
        EK::Point_3 p2(v2->point().x(), v2->point().y(), v2->info().w);

        size_t s_idx = soup_points.size();
        soup_points.push_back(p0);
        soup_points.push_back(p2);
        soup_points.push_back(p1);
        soup_polygons.push_back({s_idx, s_idx + 1, s_idx + 2});
    }

    // 9. Add outer stock box sidewalls and top ceiling
    auto is_on_stock_border = [&](const CDT_Kernel::Point_2& p) -> bool {
        return (p.x() == stock.u_min || p.x() == stock.u_max ||
                p.y() == stock.v_min || p.y() == stock.v_max);
    };

    auto is_same_stock_wall = [&](const CDT_Kernel::Point_2& pa, const CDT_Kernel::Point_2& pb) -> bool {
        if (pa.x() == stock.u_min && pb.x() == stock.u_min) return true;
        if (pa.x() == stock.u_max && pb.x() == stock.u_max) return true;
        if (pa.y() == stock.v_min && pb.y() == stock.v_min) return true;
        if (pa.y() == stock.v_max && pb.y() == stock.v_max) return true;
        return false;
    };

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->info().in_domain) continue;
        for (int i = 0; i < 3; ++i) {
            auto nh = fit->neighbor(i);
            if (!nh->info().in_domain) {
                // Border edge of in_domain
                auto va = fit->vertex((i + 1) % 3);
                auto vb = fit->vertex((i + 2) % 3);
                if (is_on_stock_border(va->point()) && is_on_stock_border(vb->point()) &&
                    is_same_stock_wall(va->point(), vb->point())) {
                    // Outer stock wall edge: add quad rising to stock.w_top
                    EK::Point_3 b_a(va->point().x(), va->point().y(), va->info().w);
                    EK::Point_3 b_b(vb->point().x(), vb->point().y(), vb->info().w);
                    EK::Point_3 t_b(vb->point().x(), vb->point().y(), stock.w_top);
                    EK::Point_3 t_a(va->point().x(), va->point().y(), stock.w_top);

                    size_t q_idx = soup_points.size();
                    soup_points.push_back(b_a);
                    soup_points.push_back(b_b);
                    soup_points.push_back(t_b);
                    soup_points.push_back(t_a);
                    soup_polygons.push_back({q_idx, q_idx + 1, q_idx + 2});
                    soup_polygons.push_back({q_idx, q_idx + 2, q_idx + 3});
                }
            }
        }
    }

    // Top ceiling quad covering the stock box
    EK::Point_3 t0(stock.u_min, stock.v_min, stock.w_top);
    EK::Point_3 t1(stock.u_max, stock.v_min, stock.w_top);
    EK::Point_3 t2(stock.u_max, stock.v_max, stock.w_top);
    EK::Point_3 t3(stock.u_min, stock.v_max, stock.w_top);

    size_t c_idx = soup_points.size();
    soup_points.push_back(t0);
    soup_points.push_back(t1);
    soup_points.push_back(t2);
    soup_points.push_back(t3);
    soup_polygons.push_back({c_idx, c_idx + 1, c_idx + 2});
    soup_polygons.push_back({c_idx, c_idx + 2, c_idx + 3});
}

} // namespace mold
} // namespace geo
} // namespace jotcad
