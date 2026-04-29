#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ClipOp : P {
    static constexpr const char* path = "jot/clip";

    struct ToolNode {
        Geometry geo;
        Matrix world_tf;
        bool is_volume;
        bool is_plane;
        bool is_pwh;
    };

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        Shape out = in;
        if (tools.empty()) {
            // Intersection with nothing is either everything or nothing? 
            // In OpenSCAD, intersection of A with nothing is nothing if there are tools.
            // But if tools is empty, we just return 'in' to be safe, or should we return empty?
            // Actually, intersection() { Box(); } in OpenSCAD is Box().
            // So if there's only one item, it's that item.
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        std::vector<ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        recursive_intersect(vfs, out, Matrix::identity(), tool_nodes);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_tool_geometry(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<ToolNode>& tool_nodes) {
        Matrix current_tf = parent_tf * s.tf;
        bool is_plane = s.tags.contains("is_plane") && s.tags["is_plane"].get<bool>();
        bool is_pwh = s.tags.contains("is_pwh") && s.tags["is_pwh"].get<bool>();
        
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            bool is_vol = !geo.faces.empty() && !is_plane && !is_pwh; 
            tool_nodes.push_back({geo, current_tf, is_vol, is_plane, is_pwh});
        }

        for (const auto& child : s.components) {
            collect_tool_geometry(vfs, child, current_tf, tool_nodes);
        }
    }

    static void recursive_intersect(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes) {
        Matrix subject_world_tf = parent_tf * s.tf;
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_only_points = target_geo.faces.empty() && target_geo.segments.empty() && !target_geo.vertices.empty();

            if (has_faces) {
                bool is_target_flat = target_geo.is_plane();

                if (is_target_flat) {
                    EK::Plane_3 target_plane = *target_geo.find_plane();
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();

                    boolean::General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, project_tf, subject_set);
                    
                    bool used_pwh_path = true;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo;
                        Matrix tool_rel_tf = subject_world_inv * tool.world_tf;
                        local_tool_geo.apply_tf(tool_rel_tf);

                        if (local_tool_geo.is_coplanar_with(target_plane)) {
                            boolean::General_polygon_set_2 tool_set;
                            add_geometry_to_gps(local_tool_geo, project_tf, tool_set);
                            boolean::Engine::clip_gps_by_gps(subject_set, tool_set);
                        } else {
                            used_pwh_path = false; break;
                        }
                    }

                    if (used_pwh_path) {
                        target_geo = boolean::Engine::gps_to_geometry(subject_set);
                        target_geo.apply_tf(rehydrate_tf);
                    } else {
                        is_target_flat = false; // Fall through to 3D
                    }
                }

                if (!is_target_flat) {
                    boolean::Surface_mesh target_mesh = boolean::Engine::geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        boolean::Surface_mesh tool_mesh = boolean::Engine::geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        boolean::Engine::transform_mesh(tool_mesh, rel_tf);
                        
                        if (CGAL::is_closed(target_mesh) && CGAL::is_closed(tool_mesh)) {
                            boolean::Engine::clip_mesh_by_mesh(target_mesh, tool_mesh);
                        } else {
                            // Non-closed or mixed dimensionality clipping is not supported yet
                        }
                    }
                    target_geo = boolean::Engine::mesh_to_geometry(target_mesh);
                }
            } else if (has_segments) {
                std::vector<std::pair<EK::Point_3, EK::Point_3>> local_segments;
                for (const auto& seg : target_geo.segments) {
                    local_segments.push_back({
                        EK::Point_3(target_geo.vertices[seg[0]].x, target_geo.vertices[seg[0]].y, target_geo.vertices[seg[0]].z),
                        EK::Point_3(target_geo.vertices[seg[1]].x, target_geo.vertices[seg[1]].y, target_geo.vertices[seg[1]].z)
                    });
                }

                for (const auto& tool : tool_nodes) {
                    if (tool.is_volume) {
                        boolean::Surface_mesh tool_mesh = boolean::Engine::geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        boolean::Engine::transform_mesh(tool_mesh, rel_tf);
                        boolean::Engine::clip_segments_by_mesh(local_segments, tool_mesh);
                    }
                }

                Geometry res;
                for (const auto& seg : local_segments) {
                    int v1 = (int)res.vertices.size();
                    res.vertices.push_back({seg.first.x(), seg.first.y(), seg.first.z()});
                    int v2 = (int)res.vertices.size();
                    res.vertices.push_back({seg.second.x(), seg.second.y(), seg.second.z()});
                    res.segments.push_back({v1, v2});
                }
                target_geo = res;
            } else if (has_only_points) {
                std::vector<EK::Point_3> pts;
                for (const auto& v : target_geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));

                for (const auto& tool : tool_nodes) {
                    if (tool.is_volume) {
                        boolean::Surface_mesh tool_mesh = boolean::Engine::geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        boolean::Engine::transform_mesh(tool_mesh, rel_tf);
                        boolean::Engine::clip_points_by_mesh(pts, tool_mesh);
                    }
                }

                Geometry res;
                for (const auto& p : pts) res.vertices.push_back({p.x(), p.y(), p.z()});
                target_geo = res;
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_intersect(vfs, child, subject_world_tf, tool_nodes);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/clip"}, 
            {"arguments", { 
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}, 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            }}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void clip_init(fs::VFSNode* vfs) {
    Processor::register_op<ClipOp<>, Shape, std::vector<Shape>>(vfs, "jot/clip");
}

} // namespace geo
} // namespace jotcad
