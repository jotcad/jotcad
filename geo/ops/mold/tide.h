#pragma once

#include "types.h"
#include "fix/assert_mesh.h"
#include "fix/soup_repair.h"
#include <CGAL/Polygon_mesh_processing/border.h>
#include <CGAL/Polygon_mesh_processing/orient_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/mark_domain_in_triangulation.h>
#include <vector>
#include <map>
#include <set>
#include <iostream>

namespace jotcad {
namespace geo {
namespace mold {

struct CDTFaceInfo {
    bool in_domain = false;
    int _nesting_level = 0;
};

template <typename Gt, typename Fb_base = CGAL::Constrained_triangulation_face_base_2<Gt>>
class CDT_Face_with_info : public Fb_base {
    CDTFaceInfo _info;
public:
    typedef Gt Geom_traits;
    typedef typename Fb_base::Vertex_handle Vertex_handle;
    typedef typename Fb_base::Face_handle   Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Fb_base::template Rebind_TDS<TDS2>::Other Fb2;
        typedef CDT_Face_with_info<Gt, Fb2> Other;
    };

    CDT_Face_with_info() : Fb_base() {}
    CDT_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2)
        : Fb_base(v0, v1, v2) {}
    CDT_Face_with_info(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2,
                       Face_handle n0, Face_handle n1, Face_handle n2)
        : Fb_base(v0, v1, v2, n0, n1, n2) {}
    CDTFaceInfo& info() { return _info; }
    const CDTFaceInfo& info() const { return _info; }
    bool is_in_domain() const { return _info.in_domain; }
    void set_in_domain(bool b) { _info.in_domain = b; }
};

struct CDTVertexInfo {
    EK::FT z = EK::FT(0);
};

template <typename Gt, typename Vb_base = CGAL::Triangulation_vertex_base_2<Gt>>
class CDT_Vertex_with_info : public Vb_base {
    CDTVertexInfo _info;
public:
    typedef Gt Geom_traits;
    typedef typename Vb_base::Point Point;
    typedef typename Vb_base::Face_handle Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Vb_base::template Rebind_TDS<TDS2>::Other Vb2;
        typedef CDT_Vertex_with_info<Gt, Vb2> Other;
    };

    CDT_Vertex_with_info() : Vb_base() {}
    CDT_Vertex_with_info(const Point& p) : Vb_base(p) {}
    CDT_Vertex_with_info(const Point& p, Face_handle f) : Vb_base(p, f) {}
    CDT_Vertex_with_info(Face_handle f) : Vb_base(f) {}
    CDTVertexInfo& info() { return _info; }
    const CDTVertexInfo& info() const { return _info; }
};

typedef CGAL::Exact_predicates_exact_constructions_kernel CDT_Kernel;
typedef CDT_Vertex_with_info<CDT_Kernel> CDT_Vb;
typedef CDT_Face_with_info<CDT_Kernel> CDT_Fb;
typedef CGAL::Triangulation_data_structure_2<CDT_Vb, CDT_Fb> CDT_TDS;
typedef CGAL::Exact_intersections_tag CDT_Itag;
typedef CGAL::Constrained_Delaunay_triangulation_2<CDT_Kernel, CDT_TDS, CDT_Itag> ExactCDT;

struct BoundarySegment3D {
    CDT_Kernel::Point_2 p1_2d;
    CDT_Kernel::Point_2 p2_2d;
    EK::FT z1;
    EK::FT z2;
};

/**
 * @brief Triangulates ruled envelope annulus between 3D boundary segments and stock box limits.
 */
inline void triangulate_margin_shelf(
    const std::vector<BoundarySegment3D>& outer_segments,
    const TideParams& tide,
    std::vector<EK::Point_3>& soup_points,
    std::vector<std::vector<size_t>>& soup_polygons
) {
    ExactCDT shelf_cdt;
    std::map<CDT_Kernel::Point_2, ExactCDT::Vertex_handle> v_handles;
    auto get_vh = [&](const CDT_Kernel::Point_2& pt, const EK::FT& z_val) {
        auto it = v_handles.find(pt);
        if (it != v_handles.end()) {
            if (z_val > it->second->info().z) {
                it->second->info().z = z_val;
            }
            return it->second;
        }
        auto vh = shelf_cdt.insert(pt);
        vh->info().z = z_val;
        v_handles[pt] = vh;
        return vh;
    };

    auto vh_c0 = get_vh(CDT_Kernel::Point_2(tide.u_min, tide.v_min), tide.z_margin);
    auto vh_c1 = get_vh(CDT_Kernel::Point_2(tide.u_max, tide.v_min), tide.z_margin);
    auto vh_c2 = get_vh(CDT_Kernel::Point_2(tide.u_max, tide.v_max), tide.z_margin);
    auto vh_c3 = get_vh(CDT_Kernel::Point_2(tide.u_min, tide.v_max), tide.z_margin);

    shelf_cdt.insert_constraint(vh_c0, vh_c1);
    shelf_cdt.insert_constraint(vh_c1, vh_c2);
    shelf_cdt.insert_constraint(vh_c2, vh_c3);
    shelf_cdt.insert_constraint(vh_c3, vh_c0);

    for (const auto& seg : outer_segments) {
        if (seg.p1_2d != seg.p2_2d) {
            auto vh_a = get_vh(seg.p1_2d, seg.z1);
            auto vh_b = get_vh(seg.p2_2d, seg.z2);
            shelf_cdt.insert_constraint(vh_a, vh_b);
        }
    }

    CGAL::mark_domain_in_triangulation(shelf_cdt);

    for (auto s_fit = shelf_cdt.finite_faces_begin(); s_fit != shelf_cdt.finite_faces_end(); ++s_fit) {
        if (!s_fit->info().in_domain) continue;

        auto v0 = s_fit->vertex(0);
        auto v1 = s_fit->vertex(1);
        auto v2 = s_fit->vertex(2);

        auto p0_3d = EK::Point_3(v0->point().x(), v0->point().y(), v0->info().z);
        auto p1_3d = EK::Point_3(v1->point().x(), v1->point().y(), v1->info().z);
        auto p2_3d = EK::Point_3(v2->point().x(), v2->point().y(), v2->info().z);

        size_t s_idx = soup_points.size();
        soup_points.push_back(p0_3d);
        soup_points.push_back(p2_3d);
        soup_points.push_back(p1_3d);
        soup_polygons.push_back({s_idx, s_idx + 1, s_idx + 2});
    }
}

/**
 * @brief Legacy 2D fallback for triangulate_margin_shelf.
 */
inline void triangulate_margin_shelf(
    const std::vector<std::pair<CDT_Kernel::Point_2, CDT_Kernel::Point_2>>& outer_segments,
    const TideParams& tide,
    std::vector<EK::Point_3>& soup_points,
    std::vector<std::vector<size_t>>& soup_polygons
) {
    std::vector<BoundarySegment3D> segs_3d;
    segs_3d.reserve(outer_segments.size());
    for (const auto& [pa, pb] : outer_segments) {
        segs_3d.push_back({pa, pb, tide.z_margin, tide.z_margin});
    }
    triangulate_margin_shelf(segs_3d, tide, soup_points, soup_polygons);
}

/**
 * @brief Adds outer stock box side walls and ceiling quad.
 */
inline void add_stock_box_outer_envelope(
    const TideParams& tide,
    std::vector<EK::Point_3>& soup_points,
    std::vector<std::vector<size_t>>& soup_polygons
) {
    auto add_quad = [&](const EK::Point_3& a, const EK::Point_3& b, const EK::Point_3& c, const EK::Point_3& d_pt) {
        size_t idx = soup_points.size();
        soup_points.push_back(a);
        soup_points.push_back(b);
        soup_points.push_back(c);
        soup_points.push_back(d_pt);
        soup_polygons.push_back({idx, idx + 1, idx + 2});
        soup_polygons.push_back({idx, idx + 2, idx + 3});
    };

    EK::Point_3 b0(tide.u_min, tide.v_min, tide.z_margin);
    EK::Point_3 b1(tide.u_max, tide.v_min, tide.z_margin);
    EK::Point_3 b2(tide.u_max, tide.v_max, tide.z_margin);
    EK::Point_3 b3(tide.u_min, tide.v_max, tide.z_margin);

    EK::Point_3 t0(tide.u_min, tide.v_min, tide.z_top);
    EK::Point_3 t1(tide.u_max, tide.v_min, tide.z_top);
    EK::Point_3 t2(tide.u_max, tide.v_max, tide.z_top);
    EK::Point_3 t3(tide.u_min, tide.v_max, tide.z_top);

    // 4 outer side walls (oriented outward)
    add_quad(b0, b1, t1, t0);
    add_quad(b1, b2, t2, t1);
    add_quad(b2, b3, t3, t2);
    add_quad(b3, b0, t0, t3);

    // Stock Ceiling (oriented outward, +Z)
    add_quad(t0, t1, t2, t3);
}

/**
 * @brief Traces border halfedges of a face patch into ordered closed 3D loops in pure EK::Point_3.
 */
inline std::vector<std::vector<EK::Point_3>> extract_patch_border_loops(
    const ExactMesh& mesh,
    const std::vector<ExactMesh::Face_index>& patch_faces
) {
    std::vector<ExactMesh::Halfedge_index> border_hes;
    CGAL::Polygon_mesh_processing::border_halfedges(patch_faces, mesh, std::back_inserter(border_hes));

    std::map<int, int> next_v;
    for (auto h : border_hes) {
        int u = (int)mesh.source(h);
        int v = (int)mesh.target(h);
        next_v[u] = v;
    }

    std::set<int> visited;
    std::vector<std::vector<EK::Point_3>> loops;

    for (auto h : border_hes) {
        int start = (int)mesh.source(h);
        if (visited.count(start)) continue;

        std::vector<int> v_chain;
        int curr = start;
        while (curr != -1 && !visited.count(curr)) {
            visited.insert(curr);
            v_chain.push_back(curr);
            auto it = next_v.find(curr);
            if (it == next_v.end()) break;
            curr = it->second;
            if (curr == start) break;
        }

        if (curr == start && v_chain.size() >= 3) {
            std::vector<EK::Point_3> loop_pts;
            loop_pts.reserve(v_chain.size());
            for (int v_idx : v_chain) {
                const auto& p = mesh.point(ExactMesh::Vertex_index(v_idx));
                loop_pts.push_back(EK::Point_3(EK::FT(p.x().exact()), EK::FT(p.y().exact()), EK::FT(p.z().exact())));
            }
            loops.push_back(std::move(loop_pts));
        }
    }
    return loops;
}

/**
 * @brief Canonical cardinal frame for the Rising Tide shelf.
 */
struct TideBasis {
    EK::Vector_3 normal; // Outward along tide direction
    EK::Vector_3 u;      // In-plane tangent axis 1
    EK::Vector_3 v;      // In-plane tangent axis 2
};

inline TideBasis compute_cardinal_tide_basis(const EK::Vector_3& d) {
    EK::FT ax = (d.x() < FT(0)) ? -d.x() : d.x();
    EK::FT ay = (d.y() < FT(0)) ? -d.y() : d.y();
    EK::FT az = (d.z() < FT(0)) ? -d.z() : d.z();

    if (az >= ax && az >= ay) {
        if (d.z() >= FT(0)) {
            return {EK::Vector_3(0, 0, 1), EK::Vector_3(1, 0, 0), EK::Vector_3(0, 1, 0)};
        } else {
            return {EK::Vector_3(0, 0, -1), EK::Vector_3(1, 0, 0), EK::Vector_3(0, -1, 0)};
        }
    } else if (ay >= ax) {
        if (d.y() >= FT(0)) {
            return {EK::Vector_3(0, 1, 0), EK::Vector_3(1, 0, 0), EK::Vector_3(0, 0, 1)};
        } else {
            return {EK::Vector_3(0, -1, 0), EK::Vector_3(1, 0, 0), EK::Vector_3(0, 0, -1)};
        }
    } else {
        if (d.x() >= FT(0)) {
            return {EK::Vector_3(1, 0, 0), EK::Vector_3(0, 1, 0), EK::Vector_3(0, 0, 1)};
        } else {
            return {EK::Vector_3(-1, 0, 0), EK::Vector_3(0, -1, 0), EK::Vector_3(0, 0, 1)};
        }
    }
}

} // namespace mold
} // namespace geo
} // namespace jotcad

#include "tide_wedge.h"
