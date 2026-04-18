#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>

namespace jotcad {
namespace geo {

typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

template <typename P = JotVfsProtocol>
struct CutOp : P {
    static constexpr const char* path = "jot/cut";

    struct ToolNode {
        Geometry geo;
        Matrix world_tf;
    };

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::vector<Shape>& tools, Shape& out) {
        if (tools.empty()) {
            out = in;
            return;
        }

        // 1. We keep the subject structure intact
        out = in;

        // 2. We build a list of all tool geometry nodes with their absolute world frames
        std::vector<ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        // 3. We recursively map the subtraction over every node in the subject tree
        recursive_subtract(vfs, out, Matrix::identity(), tool_nodes);
        out.add_tag("operation", "cut");
    }

    static void collect_tool_geometry(jotcad::fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<ToolNode>& tool_nodes) {
        Matrix current_tf = parent_tf * Matrix::from_vec(s.tf);
        if (s.geometry.has_value()) {
            Geometry geo = vfs->template read<Geometry>({s.geometry->path, s.geometry->parameters});
            tool_nodes.push_back({geo, current_tf});
        }
        for (const auto& child : s.components) {
            collect_tool_geometry(vfs, child, current_tf, tool_nodes);
        }
    }

    static void recursive_subtract(jotcad::fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        Matrix subject_world_tf = parent_tf * Matrix::from_vec(s.tf);
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            // Read local geometry (stays in its local frame)
            Geometry local_geo = vfs->template read<Geometry>({s.geometry->path, s.geometry->parameters});
            
            // Build the local GPS for this specific subject node
            General_polygon_set_2 subject_set;
            add_geometry_to_gps(local_geo, Matrix::identity(), subject_set);

            // Subtract each tool node by mapping its global geometry into THIS subject's local space
            for (const auto& tool : tool_nodes) {
                // Relative transform: SubjectWorldInv * ToolWorld
                Matrix relative_tf = subject_world_inv * tool.world_tf;
                
                General_polygon_set_2 tool_set;
                add_geometry_to_gps(tool.geo, relative_tf, tool_set);
                subject_set.difference(tool_set);
            }

            // Convert back to local geometry and update
            Geometry result_geo = gps_to_geometry(subject_set);
            s.geometry = vfs->write_geometry(result_geo);
        }

        for (auto& child : s.components) {
            recursive_subtract(vfs, child, subject_world_tf, tool_nodes);
        }
    }

    static void add_geometry_to_gps(const Geometry& geo, const Matrix& tf, General_polygon_set_2& gps) {
        // We must apply the transform to the vertices before adding to GPS
        Geometry transformed = geo;
        transformed.apply_tf(tf.to_vec());

        for (const auto& face : transformed.faces) {
            if (face.loops.empty()) continue;
            Polygon_2 boundary;
            for (int idx : face.loops[0]) {
                if (idx >= 0 && idx < (int)transformed.vertices.size())
                    boundary.push_back(Point_2(transformed.vertices[idx].x, transformed.vertices[idx].y));
            }
            if (boundary.is_empty()) continue;
            if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();

            std::vector<Polygon_2> holes;
            for (size_t i = 1; i < face.loops.size(); ++i) {
                Polygon_2 h;
                for (int idx : face.loops[i]) {
                    if (idx >= 0 && idx < (int)transformed.vertices.size())
                        h.push_back(Point_2(transformed.vertices[idx].x, transformed.vertices[idx].y));
                }
                if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                holes.push_back(h);
            }
            gps.join(Polygon_with_holes_2(boundary, holes.begin(), holes.end()));
        }
    }

    static Geometry gps_to_geometry(const General_polygon_set_2& gps) {
        Geometry g;
        std::vector<Polygon_with_holes_2> pwhs;
        gps.polygons_with_holes(std::back_inserter(pwhs));
        for (const auto& pwh : pwhs) {
            Geometry::Face face;
            std::vector<int> outer;
            for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                outer.push_back((int)g.vertices.size());
                g.vertices.push_back({it->x(), it->y(), FT(0)});
            }
            if (!outer.empty()) face.loops.push_back(outer);
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                std::vector<int> hole;
                for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                    hole.push_back((int)g.vertices.size());
                    g.vertices.push_back({it->x(), it->y(), FT(0)});
                }
                if (!hole.empty()) face.loops.push_back(hole);
            }
            g.faces.push_back(face);
        }
        return g;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"tools", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}, {"tools", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void cut_init() {
    Processor::register_op<CutOp<>, Shape, Shape, std::vector<Shape>>();
}

} // namespace geo
} // namespace jotcad
