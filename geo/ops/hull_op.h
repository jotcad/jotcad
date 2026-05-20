#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/convex_hull_2.h>
#include <CGAL/convex_hull_3.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct HullOp : P {
    static void collect_points(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<EK::Point_3>& pts) {
        Matrix current_tf = parent_tf * s.tf;
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            for (const auto& v : geo.vertices) {
                pts.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
            }
        }
        for (const auto& child : s.components) {
            collect_points(vfs, child, current_tf, pts);
        }
    }

    static void execute_hull(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<EK::Point_3>& pts, const nlohmann::json& base_tags = {}) {
        if (pts.size() < 3) {
             vfs->write(fulfilling.with_output("$out"), Shape());
             return;
        }

        // Check for coplanarity
        bool coplanar = true;
        EK::Plane_3 plane;
        bool plane_set = false;

        // Try to find a valid plane from first 3 non-collinear points
        for (size_t i = 0; i < pts.size() && !plane_set; ++i) {
            for (size_t j = i + 1; j < pts.size() && !plane_set; ++j) {
                for (size_t k = j + 1; k < pts.size() && !plane_set; ++k) {
                    EK::Plane_3 p(pts[i], pts[j], pts[k]);
                    if (!p.is_degenerate()) {
                        plane = p;
                        plane_set = true;
                    }
                }
            }
        }

        if (!plane_set) {
            // All points are collinear or identical. Return empty for now.
            vfs->write(fulfilling.with_output("$out"), Shape());
            return;
        }

        for (const auto& p : pts) {
            if (!plane.has_on(p)) {
                coplanar = false;
                break;
            }
        }

        Geometry res;
        if (coplanar) {
            // 2D Convex Hull
            Matrix proj = Matrix::lookAt(plane.point(), plane.orthogonal_vector());
            std::vector<EK::Point_2> pts2d;
            for (const auto& p : pts) {
                EK::Point_3 lp = proj.transform(p);
                pts2d.push_back(EK::Point_2(lp.x(), lp.y()));
            }

            std::vector<EK::Point_2> hull_pts;
            CGAL::convex_hull_2(pts2d.begin(), pts2d.end(), std::back_inserter(hull_pts));

            std::vector<int> loop;
            Matrix inv = proj.inverse();
            for (const auto& p : hull_pts) {
                int idx = (int)res.vertices.size();
                EK::Point_3 wp = inv.transform(EK::Point_3(p.x(), p.y(), 0));
                res.vertices.push_back({wp.x(), wp.y(), wp.z()});
                loop.push_back(idx);
            }
            if (!loop.empty()) {
                Geometry::Face nf;
                nf.loops.push_back(loop);
                res.faces.push_back(nf);
            }
        } else {
            // 3D Convex Hull
            boolean::Surface_mesh sm;
            CGAL::convex_hull_3(pts.begin(), pts.end(), sm);
            res = boolean::Engine::mesh_to_geometry(sm);
        }

        Shape out;
        out.tags = base_tags;
        out.add_tag("type", coplanar ? "surface" : "closed");
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    struct Constructor {
        static constexpr const char* path = "jot/Hull";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
            std::vector<EK::Point_3> pts;
            for (const auto& s : shapes) {
                collect_points(vfs, s, Matrix::identity(), pts);
            }
            execute_hull(vfs, fulfilling, pts);
        }
        static std::vector<std::string> argument_keys() { return {"shapes"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates the convex hull of multiple shapes."},
                {"arguments", {{{"name", "shapes"}, {"type", "jot:shapes"}}}},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };

    struct Method {
        static constexpr const char* path = "jot/hull";
        static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
            std::vector<EK::Point_3> pts;
            collect_points(vfs, in, Matrix::identity(), pts);
            execute_hull(vfs, fulfilling, pts, in.tags);
        }
        static std::vector<std::string> argument_keys() { return {"$in"}; }
        static typename P::json schema() {
            return {
                {"path", path},
                {"description", "Generates the convex hull of the input shape (including children)."},
                {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to hull."}}}}},
                {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
            };
        }
    };
};

static void hull_init(fs::VFSNode* vfs) {
    Processor::register_op<HullOp<>::Constructor, std::vector<Shape>>(vfs, "jot/Hull");
    Processor::register_op<HullOp<>::Method, Shape>(vfs, "jot/hull");
}

} // namespace geo
} // namespace jotcad
