#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/alpha_wrap_3.h>
#include <CGAL/Cartesian_converter.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct WrapOp : P {
    static constexpr const char* path = "jot/wrap";

    static void collect_for_wrap(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, 
                                 std::vector<EK::Point_3>& all_pts, 
                                 std::vector<std::vector<size_t>>& all_tris) {
        Matrix current_tf = parent_tf * s.tf;
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            size_t v_base = all_pts.size();

            for (const auto& v : geo.vertices) {
                all_pts.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
            }

            // 1. Existing Triangles
            for (const auto& tri : geo.triangles) {
                all_tris.push_back({v_base + tri[0], v_base + tri[1], v_base + tri[2]});
            }

            // 2. Faces (convert to triangles)
            for (const auto& f : geo.faces) {
                if (f.loops.empty()) continue;
                // Simple fan triangulation for wrap input (doesn't need to be perfect, just provide surface)
                const auto& loop = f.loops[0];
                for (size_t i = 1; i + 1 < loop.size(); ++i) {
                    all_tris.push_back({v_base + loop[0], v_base + loop[i], v_base + loop[i+1]});
                }
            }

            // 3. Segments (as tiny triangles)
            FT iota = FT(1) / FT(1000000000LL);
            for (const auto& seg : geo.segments) {
                size_t s_idx = v_base + seg[0];
                size_t t_idx = v_base + seg[1];
                const auto& tp = geo.vertices[seg[1]];
                size_t ti_idx = all_pts.size();
                all_pts.push_back(current_tf.transform(EK::Point_3(tp.x + iota, tp.y + iota, tp.z + iota)));
                all_tris.push_back({s_idx, t_idx, ti_idx});
            }

            // 4. Points (as tiny triangles)
            for (int p_idx : geo.points) {
                const auto& p = geo.vertices[p_idx];
                size_t idx0 = v_base + p_idx;
                size_t idx1 = all_pts.size();
                all_pts.push_back(current_tf.transform(EK::Point_3(p.x + iota, p.y, p.z)));
                size_t idx2 = all_pts.size();
                all_pts.push_back(current_tf.transform(EK::Point_3(p.x, p.y + iota, p.z)));
                all_tris.push_back({idx0, idx1, idx2});
            }
        }
        for (const auto& child : s.components) {
            collect_for_wrap(vfs, child, current_tf, all_pts, all_tris);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double alpha, double offset) {
        std::vector<EK::Point_3> pts_ek;
        std::vector<std::vector<size_t>> tris;
        collect_for_wrap(vfs, in, Matrix::identity(), pts_ek, tris);

        if (pts_ek.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // Convert to Inexact Kernel for CGAL::alpha_wrap_3
        CGAL::Cartesian_converter<EK, IK> ek_to_ik;
        std::vector<IK::Point_3> pts_ik;
        pts_ik.reserve(pts_ek.size());
        for (const auto& p : pts_ek) pts_ik.push_back(ek_to_ik(p));

        CGAL::Surface_mesh<IK::Point_3> sm_ik;
        if (tris.empty()) {
            CGAL::alpha_wrap_3(pts_ik, alpha, offset, sm_ik);
        } else {
            CGAL::alpha_wrap_3(pts_ik, tris, alpha, offset, sm_ik);
        }

        // Convert back to EK
        CGAL::Cartesian_converter<IK, EK> ik_to_ek;
        Geometry res;
        std::map<CGAL::Surface_mesh<IK::Point_3>::Vertex_index, int> v_map;
        for (auto v : sm_ik.vertices()) {
            v_map[v] = (int)res.vertices.size();
            auto p = ik_to_ek(sm_ik.point(v));
            res.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for (auto f : sm_ik.faces()) {
            Geometry::Face face;
            std::vector<int> loop;
            for (auto v : sm_ik.vertices_around_face(sm_ik.halfedge(f))) {
                loop.push_back(v_map[v]);
            }
            face.loops.push_back(loop);
            res.faces.push_back(face);
        }

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "wrap");
        out.add_tag("alpha", alpha);
        out.add_tag("offset", offset);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "alpha", "offset"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/wrap"},
            {"description", "Generates a tight manifold 'alpha wrap' around the input geometry."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "alpha"}, {"type", "jot:number"}, {"default", 1.0}, {"description", "The alpha parameter (tightness). Smaller is tighter."}},
                {{"name", "offset"}, {"type", "jot:number"}, {"default", 0.1}, {"description", "Offset from the input points."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void wrap_init(fs::VFSNode* vfs) {
    Processor::register_op<WrapOp<>, Shape, double, double>(vfs, "jot/wrap");
}

} // namespace geo
} // namespace jotcad
