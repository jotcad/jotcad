#pragma once

#include "protocols.h"
#include "matrix.h"
#include "boolean/engine.h"
#include <CGAL/Arrangement_2.h>
#include <CGAL/Arr_segment_traits_2.h>
#include <CGAL/Arr_extended_dcel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_mesh_processing/self_intersections.h>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace jotcad {
namespace geo {
namespace extrude {

typedef CGAL::Arr_segment_traits_2<EK> Arr_traits_2;
typedef CGAL::Arr_extended_dcel<Arr_traits_2, int, int, int> Arr_dcel;
typedef CGAL::Arrangement_2<Arr_traits_2, Arr_dcel> Arrangement_2;

struct PolylineExtruder {
    struct Frame {
        Vector_3 u;
        Vector_3 v;
        Vector_3 d;
        EK::FT d_sq;
        EK::FT u_sq;
        EK::FT v_sq;
    };

    static Frame compute_frame(const Vector_3& d_in) {
        Frame f;
        f.d = d_in;
        f.d_sq = f.d.squared_length();
        if (f.d_sq == EK::FT(0)) {
            f.d = Vector_3(0, 0, 1);
            f.d_sq = EK::FT(1);
        }
        if (CGAL::abs(f.d.x()) > CGAL::abs(f.d.z())) {
            f.u = Vector_3(-f.d.y(), f.d.x(), EK::FT(0));
        } else {
            f.u = Vector_3(EK::FT(0), -f.d.z(), f.d.y());
        }
        f.v = CGAL::cross_product(f.d, f.u);
        f.u_sq = f.u.squared_length();
        f.v_sq = f.v.squared_length();
        return f;
    }

    static EK::Point_2 project_to_2d(const Point_3& p, const Frame& f) {
        EK::FT u_coord = (p.x() * f.u.x() + p.y() * f.u.y() + p.z() * f.u.z()) / f.u_sq;
        EK::FT v_coord = (p.x() * f.v.x() + p.y() * f.v.y() + p.z() * f.v.z()) / f.v_sq;
        return EK::Point_2(u_coord, v_coord);
    }

    static EK::FT get_z_along_d(const Point_3& p, const Frame& f) {
        return (p.x() * f.d.x() + p.y() * f.d.y() + p.z() * f.d.z()) / f.d_sq;
    }

    static std::vector<boolean::ExactMesh> extrude_segments(
        const Geometry& in_geo,
        const Matrix& bottom_tf,
        const Matrix& top_tf)
    {
        std::vector<boolean::ExactMesh> result_meshes;
        if (in_geo.segments.empty()) return result_meshes;

        // Extract 3D segments
        struct Seg3D {
            Point_3 p0, p1;
            EK::Point_2 p0_2d, p1_2d;
            EK::FT z0, z1;
            bool is_vertical;
        };
        std::vector<Seg3D> segs_3d;
        segs_3d.reserve(in_geo.segments.size());

        Point_3 origin_bot = bottom_tf.transform(Point_3(0, 0, 0));
        Point_3 origin_top = top_tf.transform(Point_3(0, 0, 0));
        Vector_3 d = origin_top - origin_bot;
        Frame frame = compute_frame(d);

        Arrangement_2 arr;
        for (const auto& s_idx : in_geo.segments) {
            Point_3 a(in_geo.vertices[s_idx[0]].x, in_geo.vertices[s_idx[0]].y, in_geo.vertices[s_idx[0]].z);
            Point_3 b(in_geo.vertices[s_idx[1]].x, in_geo.vertices[s_idx[1]].y, in_geo.vertices[s_idx[1]].z);
            Seg3D s;
            s.p0 = a;
            s.p1 = b;
            s.p0_2d = project_to_2d(a, frame);
            s.p1_2d = project_to_2d(b, frame);
            s.z0 = get_z_along_d(a, frame);
            s.z1 = get_z_along_d(b, frame);
            s.is_vertical = (s.p0_2d == s.p1_2d);
            segs_3d.push_back(s);

            if (!s.is_vertical) {
                CGAL::insert(arr, EK::Segment_2(s.p0_2d, s.p1_2d));
            }
        }

        // Helper to find parent 3D segment for a 2D halfedge
        auto find_parent_seg = [&](const EK::Point_2& src, const EK::Point_2& tgt) -> int {
            EK::Segment_2 query_seg(src, tgt);
            for (size_t i = 0; i < segs_3d.size(); ++i) {
                if (segs_3d[i].is_vertical) continue;
                EK::Segment_2 parent_2d(segs_3d[i].p0_2d, segs_3d[i].p1_2d);
                if (parent_2d.has_on(src) && parent_2d.has_on(tgt)) {
                    return (int)i;
                }
            }
            return -1;
        };

        // Extrude each bounded face of the arrangement as a separate mesh
        for (auto f = arr.faces_begin(); f != arr.faces_end(); ++f) {
            if (f->is_unbounded()) continue;

            // Extract the simple CCB loop
            std::vector<Arrangement_2::Halfedge_const_handle> ccb_edges;
            auto circ = f->outer_ccb();
            auto curr = circ;
            do {
                ccb_edges.push_back(curr);
            } while (++curr != circ);

            if (ccb_edges.size() < 3) continue;

            // Unproject CCB vertices to 3D with double-quad elevation step resolution
            boolean::ExactMesh em;
            std::map<Point_3, boolean::ExactMesh::Vertex_index> v_map;
            auto get_v = [&](const Point_3& p) -> boolean::ExactMesh::Vertex_index {
                auto it = v_map.find(p);
                if (it != v_map.end()) return it->second;
                auto vd = em.add_vertex(p);
                v_map[p] = vd;
                return vd;
            };

            for (size_t i = 0; i < ccb_edges.size(); ++i) {
                auto he = ccb_edges[i];
                auto he_next = ccb_edges[(i + 1) % ccb_edges.size()];

                EK::Point_2 src = he->source()->point();
                EK::Point_2 tgt = he->target()->point();
                EK::Point_2 tgt_next = he_next->target()->point();

                int p_idx = find_parent_seg(src, tgt);
                int p_next_idx = find_parent_seg(tgt, tgt_next);

                // Compute exact 3D coordinates along originating segment
                auto unproject_point = [&](const EK::Point_2& pt, int seg_i) -> Point_3 {
                    if (seg_i < 0 || seg_i >= (int)segs_3d.size()) {
                        return Point_3(pt.x(), pt.y(), EK::FT(0));
                    }
                    const auto& s = segs_3d[seg_i];
                    EK::Vector_2 seg_v = s.p1_2d - s.p0_2d;
                    EK::FT len_sq = seg_v.squared_length();
                    if (len_sq == EK::FT(0)) return s.p0;
                    EK::Vector_2 diff = pt - s.p0_2d;
                    EK::FT dot = diff.x() * seg_v.x() + diff.y() * seg_v.y();
                    EK::FT t = dot / len_sq;
                    Vector_3 v3 = s.p1 - s.p0;
                    return Point_3(s.p0.x() + v3.x() * t, s.p0.y() + v3.y() * t, s.p0.z() + v3.z() * t);
                };

                Point_3 p_src = unproject_point(src, p_idx);
                Point_3 p_tgt = unproject_point(tgt, p_idx);

                // Transform bottom and top
                Point_3 b_src = bottom_tf.transform(p_src);
                Point_3 b_tgt = bottom_tf.transform(p_tgt);
                Point_3 t_src = top_tf.transform(p_src);
                Point_3 t_tgt = top_tf.transform(p_tgt);

                auto v_bs = get_v(b_src);
                auto v_bt = get_v(b_tgt);
                auto v_tt = get_v(t_tgt);
                auto v_ts = get_v(t_src);

                Point_3 p_tgt_next = unproject_point(tgt, p_next_idx);
                EK::FT z_tgt = get_z_along_d(p_tgt, frame);
                EK::FT z_tgt_next = get_z_along_d(p_tgt_next, frame);

                if (z_tgt < z_tgt_next) {
                    // Outgoing segment is higher at tgt:
                    // Subdivide end edge at b_step to share edge [v_bstep, v_tt] with next quad
                    Point_3 b_step = bottom_tf.transform(p_tgt_next);
                    auto v_bstep = get_v(b_step);

                    if (v_bs != v_bt && v_bt != v_bstep && v_bstep != v_bs) {
                        em.add_face(v_bs, v_bt, v_bstep);
                    }
                    if (v_bs != v_bstep && v_bstep != v_tt && v_tt != v_bs) {
                        em.add_face(v_bs, v_bstep, v_tt);
                    }
                    if (v_bs != v_tt && v_tt != v_ts && v_ts != v_bs) {
                        em.add_face(v_bs, v_tt, v_ts);
                    }
                } else if (z_tgt > z_tgt_next) {
                    // Outgoing segment is lower at tgt:
                    // Standard quad; next quad will subdivide its start edge at v_bt
                    if (v_bs != v_bt && v_bt != v_tt && v_tt != v_bs) {
                        em.add_face(v_bs, v_bt, v_tt);
                    }
                    if (v_bs != v_tt && v_tt != v_ts && v_ts != v_bs) {
                        em.add_face(v_bs, v_tt, v_ts);
                    }
                } else {
                    // Continuous elevation across tgt
                    if (v_bs != v_bt && v_bt != v_tt && v_tt != v_bs) {
                        em.add_face(v_bs, v_bt, v_tt);
                    }
                    if (v_bs != v_tt && v_tt != v_ts && v_ts != v_bs) {
                        em.add_face(v_bs, v_tt, v_ts);
                    }
                }
            }

            if (!em.is_empty()) {
                result_meshes.push_back(em);
            }
        }

        return result_meshes;
    }
};

} // namespace extrude
} // namespace geo
} // namespace jotcad
