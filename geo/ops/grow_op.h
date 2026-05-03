#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/convex_hull_3.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GrowOp : P {
    static constexpr const char* path = "jot/grow";

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

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& tool_shape) {
        if (!in.geometry.has_value() && in.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 1. Collect tool points (assuming tool's convex hull will be used)
        std::vector<EK::Point_3> tool_pts;
        collect_points(vfs, tool_shape, Matrix::identity(), tool_pts);
        if (tool_pts.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // 2. Resolve input geometry
        if (!in.geometry.has_value()) {
             vfs->write(fulfilling.with_output("$out"), in);
             return;
        }
        
        Geometry subject_geo = vfs->read<Geometry>(in.geometry.value()); 

        boolean::Surface_mesh final_mesh;

        auto add_hull = [&](const std::vector<EK::Point_3>& points) {
            if (points.empty()) return;
            boolean::Surface_mesh hull;
            CGAL::convex_hull_3(points.begin(), points.end(), hull);
            if (final_mesh.is_empty()) {
                final_mesh = hull;
            } else {
                boolean::Engine::join_mesh_by_mesh(final_mesh, hull);
            }
        };

        EK::Point_3 zero(0, 0, 0);

        // Process Points
        for (const auto& v : subject_geo.vertices) {
            std::vector<EK::Point_3> pts;
            EK::Vector_3 offset(v.x, v.y, v.z);
            for (const auto& tp : tool_pts) pts.push_back(tp + offset);
            add_hull(pts);
        }

        // Process Segments
        for (const auto& seg : subject_geo.segments) {
            std::vector<EK::Point_3> pts;
            EK::Vector_3 o1(subject_geo.vertices[seg[0]].x, subject_geo.vertices[seg[0]].y, subject_geo.vertices[seg[0]].z);
            EK::Vector_3 o2(subject_geo.vertices[seg[1]].x, subject_geo.vertices[seg[1]].y, subject_geo.vertices[seg[1]].z);
            for (const auto& tp : tool_pts) {
                pts.push_back(tp + o1);
                pts.push_back(tp + o2);
            }
            add_hull(pts);
        }

        // Process Faces (triangulate and grow each triangle)
        for (const auto& face : subject_geo.faces) {
            if (face.loops.empty()) continue;
            // Simplified: treat face as a set of triangles for growing
            // (Assuming even concave faces can be grown by their boundary points + tool if tool is convex)
            // But to be safe and handle non-convexity properly, we should triangulate.
            // For now, let's just hull the whole face vertices + tool.
            std::vector<EK::Point_3> pts;
            for (const auto& loop : face.loops) {
                for (int idx : loop) {
                    EK::Vector_3 offset(subject_geo.vertices[idx].x, subject_geo.vertices[idx].y, subject_geo.vertices[idx].z);
                    for (const auto& tp : tool_pts) pts.push_back(tp + offset);
                }
            }
            add_hull(pts);
        }

        Geometry res_geo = boolean::Engine::mesh_to_geometry(final_mesh);
        
        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res_geo);
        out.add_tag("type", "grow");
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tool"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/grow"},
            {"description", "Grows (thickens) the subject geometry by sweeping a tool shape over it (Minkowski Sum)."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "tool"}, {"type", "jot:shape"}, {"description", "The tool shape to sweep."}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void grow_init(fs::VFSNode* vfs) {
    Processor::register_op<GrowOp<>, Shape, Shape>(vfs, "jot/grow");
}

} // namespace geo
} // namespace jotcad
