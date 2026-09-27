#pragma once

#include "types.h"
#include "fix/assert_mesh.h"
#include "fix/soup_repair.h"
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Delaunay_mesh_vertex_base_2.h>
#include <CGAL/Delaunay_mesh_face_base_2.h>
#include <CGAL/Delaunay_mesher_2.h>
#include <CGAL/Delaunay_mesh_size_criteria_2.h>
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
    double max_edge_len = 0.0; // 0.0 = unrefined CDT; > 0.0 = isotropic refinement
};

struct HarmonicVertexInfo {
    double w = 0.0;
    EK::FT exact_w = EK::FT(0);
    bool is_fixed = false;
    int index = -1;
};

template <typename Gt, typename Vb_base = CGAL::Delaunay_mesh_vertex_base_2<Gt>>
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

template <typename Gt, typename Fb_base = CGAL::Delaunay_mesh_face_base_2<Gt>>
class Harmonic_Face_with_info : public Fb_base {
    int _nesting_level = 0;
public:
    typedef Gt Geom_traits;
    typedef typename Fb_base::Vertex_handle Vertex_handle;
    typedef typename Fb_base::Face_handle   Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Fb_base::template Rebind_TDS<TDS2>::Other Fb2;
        typedef Harmonic_Face_with_info<Gt, Fb2> Other;
    };

    Harmonic_Face_with_info() : Fb_base() {}
    Harmonic_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2)
        : Fb_base(v0, v1, v2) {}
    Harmonic_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2,
                            Face_handle n0, Face_handle n1, Face_handle n2)
        : Fb_base(v0, v1, v2, n0, n1, n2) {}

    int nesting_level() const { return _nesting_level; }
    void set_nesting_level(int l) { _nesting_level = l; }
};

typedef Harmonic_Vertex_with_info<CDT_Kernel> Harmonic_Vb;
typedef Harmonic_Face_with_info<CDT_Kernel> Harmonic_Fb;
typedef CGAL::Triangulation_data_structure_2<Harmonic_Vb, Harmonic_Fb> Harmonic_TDS;
typedef CGAL::Constrained_Delaunay_triangulation_2<CDT_Kernel, Harmonic_TDS, CDT_Itag> HarmonicCDT;
typedef CGAL::Delaunay_mesh_size_criteria_2<HarmonicCDT> HarmonicCriteria;
typedef CGAL::Delaunay_mesher_2<HarmonicCDT, HarmonicCriteria> HarmonicMesher;

/**
 * @brief Constructs a closed, watertight 2-manifold solid wedge directly as an ExactMesh
 *        via soup-free prismatic halfedge extrusion over the 2D CDT domain.
 * 
 * Solves the discrete Dirichlet energy / Laplace equation Delta w = 0 with natural Neumann
 * boundary condition on stock walls (orthogonal flush exit) and obstacle clearance constraints.
 */
inline ExactMesh construct_harmonic_wedge(
    const std::vector<BoundarySegment3D>& inner_segments,
    const HarmonicStockParams& stock,
    const std::vector<EK::Point_3>& interior_patch_points = {},
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
                it->second->info().w = CGAL::to_double(init_w);
                it->second->info().exact_w = init_w;
            }
            return it->second;
        }
        auto vh = cdt.insert(pt);
        vh->info().w = CGAL::to_double(init_w);
        vh->info().exact_w = init_w;
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

    // 2. Insert any optional interior patch vertices
    for (const auto& pt : interior_patch_points) {
        get_or_insert(CDT_Kernel::Point_2(pt.x(), pt.y()), pt.z(), true);
    }

    // 3. Insert inner model patch boundary obligations (fixed Dirichlet boundary)
    std::vector<BoundarySegment3D> all_boundary_segments;
    double sum_fixed_w = 0.0;
    size_t count_fixed_w = 0;
    for (const auto& seg : inner_segments) {
        if (seg.p1_2d != seg.p2_2d) {
            all_boundary_segments.push_back(seg);
            auto vh_a = get_or_insert(seg.p1_2d, seg.z1, true);
            auto vh_b = get_or_insert(seg.p2_2d, seg.z2, true);
            cdt.insert_constraint(vh_a, vh_b);
            sum_fixed_w += CGAL::to_double(seg.z1) + CGAL::to_double(seg.z2);
            count_fixed_w += 2;
        }
    }

    // 4. Insert prior piece parting seams (fixed Dirichlet boundary)
    for (const auto& seg : prior_seams) {
        if (seg.p1_2d != seg.p2_2d) {
            all_boundary_segments.push_back(seg);
            auto vh_a = get_or_insert(seg.p1_2d, seg.z1, true);
            auto vh_b = get_or_insert(seg.p2_2d, seg.z2, true);
            cdt.insert_constraint(vh_a, vh_b);
            sum_fixed_w += CGAL::to_double(seg.z1) + CGAL::to_double(seg.z2);
            count_fixed_w += 2;
        }
    }

    double avg_fixed_w = (count_fixed_w > 0) ? (sum_fixed_w / static_cast<double>(count_fixed_w)) : 0.0;

    // 5. Mark domain in triangulation (annular region between inner patch and outer stock)
    CGAL::mark_domain_in_triangulation(cdt);

    // 6. Optional Isotropic Delaunay Refinement of the free space domain
    if (stock.max_edge_len > 0.0) {
        HarmonicCriteria criteria(0.125, stock.max_edge_len);
        HarmonicMesher mesher(cdt, criteria);
        mesher.init(true); // domain_specified = true
        mesher.refine_mesh();
    }

    // 7. Audit and pin all vertices post-refinement
    for (auto vit = cdt.finite_vertices_begin(); vit != cdt.finite_vertices_end(); ++vit) {
        const auto& pt = vit->point();
        bool is_pinned = false;
        for (const auto& seg : all_boundary_segments) {
            if (CGAL::collinear(seg.p1_2d, pt, seg.p2_2d)) {
                EK::FT t(0);
                bool inside = false;
                if (seg.p1_2d.x() != seg.p2_2d.x()) {
                    t = (pt.x() - seg.p1_2d.x()) / (seg.p2_2d.x() - seg.p1_2d.x());
                    if (t >= EK::FT(0) && t <= EK::FT(1)) inside = true;
                } else if (seg.p1_2d.y() != seg.p2_2d.y()) {
                    t = (pt.y() - seg.p1_2d.y()) / (seg.p2_2d.y() - seg.p1_2d.y());
                    if (t >= EK::FT(0) && t <= EK::FT(1)) inside = true;
                }
                if (inside) {
                    EK::FT z_exact = seg.z1 + t * (seg.z2 - seg.z1);
                    vit->info().is_fixed = true;
                    vit->info().exact_w = z_exact;
                    vit->info().w = CGAL::to_double(z_exact);
                    is_pinned = true;
                    break;
                }
            }
        }
        if (!is_pinned && !vit->info().is_fixed) {
            vit->info().is_fixed = false;
            vit->info().exact_w = EK::FT(0);
            vit->info().w = avg_fixed_w;
        }
    }

    // 8. Collect all active vertices belonging to in_domain faces
    std::vector<HarmonicCDT::Vertex_handle> active_vertices;
    std::set<HarmonicCDT::Vertex_handle> active_set;

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (!fit->is_in_domain()) continue;
        for (int i = 0; i < 3; ++i) {
            auto v = fit->vertex(i);
            if (active_set.insert(v).second) {
                v->info().index = (int)active_vertices.size();
                active_vertices.push_back(v);
            }
        }
    }

    // 9. Build adjacency graph with double inverse-distance-squared weights
    size_t V = active_vertices.size();
    if (V > 0) {
        std::vector<std::vector<std::pair<size_t, double>>> adj(V);
        std::set<std::pair<size_t, size_t>> seen_edges;

        for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
            if (!fit->is_in_domain()) continue;
            for (int i = 0; i < 3; ++i) {
                auto v1 = fit->vertex(i);
                auto v2 = fit->vertex((i + 1) % 3);
                size_t idx1 = (size_t)v1->info().index;
                size_t idx2 = (size_t)v2->info().index;
                if (idx1 > idx2) std::swap(idx1, idx2);
                if (seen_edges.insert({idx1, idx2}).second) {
                    double dx = CGAL::to_double(v1->point().x() - v2->point().x());
                    double dy = CGAL::to_double(v1->point().y() - v2->point().y());
                    double d2 = dx * dx + dy * dy;
                    if (d2 > 1e-14) {
                        double weight = 1.0 / d2;
                        adj[idx1].push_back({idx2, weight});
                        adj[idx2].push_back({idx1, weight});
                    }
                }
            }
        }

        // 10. Solve discrete Laplace equation via Projected Gauss-Seidel
        for (int iter = 0; iter < max_iterations; ++iter) {
            for (size_t i = 0; i < V; ++i) {
                if (active_vertices[i]->info().is_fixed) continue;
                double sum_w = 0.0;
                double sum_wv = 0.0;
                for (const auto& [j, w_ij] : adj[i]) {
                    sum_w += w_ij;
                    sum_wv += w_ij * active_vertices[j]->info().w;
                }
                if (sum_w > 0.0) {
                    double new_w = sum_wv / sum_w;
                    if (obstacle_fn) {
                        auto obs = obstacle_fn(active_vertices[i]->point());
                        if (obs) {
                            double obs_val = CGAL::to_double(*obs);
                            if (obs_val > new_w) new_w = obs_val;
                        }
                    }
                    active_vertices[i]->info().w = new_w;
                }
            }
        }

    }

    // 10.5 Round final solved heights to nearest 0.01 mm (exact centi / 100)
    for (auto vit = cdt.finite_vertices_begin(); vit != cdt.finite_vertices_end(); ++vit) {
        if (!vit->info().is_fixed) {
            long long centi = std::llround(vit->info().w * 100.0);
            vit->info().w = static_cast<double>(centi) / 100.0;
            vit->info().exact_w = EK::FT(centi) / EK::FT(100);
        }
    }

    // 11. Direct Prismatic Halfedge Mesh Construction
    // Duplicates finite CDT vertices into bottom w(u,v) and top w_top.
    // Bottom faces are oriented CW (-Z normal), top faces CCW (+Z normal),
    // and outer stock walls are vertical quads. Euler characteristic chi = 2 identically.
    std::vector<HarmonicCDT::Vertex_handle> all_finite_verts;
    std::map<HarmonicCDT::Vertex_handle, size_t> vert_idx;

    for (auto vit = cdt.finite_vertices_begin(); vit != cdt.finite_vertices_end(); ++vit) {
        vert_idx[vit] = all_finite_verts.size();
        all_finite_verts.push_back(vit);
    }

    size_t num_v = all_finite_verts.size();
    ExactMesh wedge_mesh;
    std::vector<ExactMesh::Vertex_index> bot_verts(num_v);
    std::vector<ExactMesh::Vertex_index> top_verts(num_v);

    for (size_t i = 0; i < num_v; ++i) {
        auto vh = all_finite_verts[i];
        EK::FT z_bot = vh->info().exact_w;
        EK::Point_3 p_bot(vh->point().x(), vh->point().y(), z_bot);
        EK::Point_3 p_top(vh->point().x(), vh->point().y(), stock.w_top);

        bot_verts[i] = wedge_mesh.add_vertex(p_bot);
        top_verts[i] = wedge_mesh.add_vertex(p_top);
    }

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        size_t i0 = vert_idx[fit->vertex(0)];
        size_t i1 = vert_idx[fit->vertex(1)];
        size_t i2 = vert_idx[fit->vertex(2)];

        // Bottom face: CW in (u, v) so normal points downward -Z
        wedge_mesh.add_face(bot_verts[i0], bot_verts[i2], bot_verts[i1]);

        // Top face: CCW in (u, v) so normal points upward +Z
        wedge_mesh.add_face(top_verts[i0], top_verts[i1], top_verts[i2]);
    }

    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        for (int i = 0; i < 3; ++i) {
            if (cdt.is_infinite(fit->neighbor(i))) {
                auto va = fit->vertex((i + 1) % 3);
                auto vb = fit->vertex((i + 2) % 3);
                size_t ia = vert_idx[va];
                size_t ib = vert_idx[vb];

                wedge_mesh.add_face(bot_verts[ia], bot_verts[ib], top_verts[ib]);
                wedge_mesh.add_face(bot_verts[ia], top_verts[ib], top_verts[ia]);
            }
        }
    }

    wedge_mesh.collect_garbage();
    if (CGAL::is_closed(wedge_mesh)) {
        CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(wedge_mesh);
    }
    return wedge_mesh;
}

/**
 * @brief Legacy adapter for polygon soup interfaces. Populates soup with the watertight
 *        mesh generated by construct_harmonic_wedge.
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
    ExactMesh mesh = construct_harmonic_wedge(inner_segments, stock, {}, obstacle_fn, prior_seams, max_iterations);
    soup_points.clear();
    soup_polygons.clear();

    for (auto v : mesh.vertices()) {
        soup_points.push_back(mesh.point(v));
    }
    for (auto f : mesh.faces()) {
        auto h = mesh.halfedge(f);
        size_t i0 = (size_t)mesh.source(h);
        size_t i1 = (size_t)mesh.target(h);
        size_t i2 = (size_t)mesh.target(mesh.next(h));
        soup_polygons.push_back({i0, i1, i2});
    }
}

} // namespace mold
} // namespace geo
} // namespace jotcad
