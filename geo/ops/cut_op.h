#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct CutOp : P {
    static constexpr const char* path = "jot/cut";

    struct ToolNode {
        Geometry geo;
        Matrix world_tf;
        bool is_volume;
        bool is_plane;
        bool is_pwh;
    };

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools, bool open = false) {
        Shape out = in;
        if (tools.empty()) {
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        std::vector<ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        recursive_subtract(vfs, out, Matrix::identity(), tool_nodes, open);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void collect_tool_geometry(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<ToolNode>& tool_nodes) {
        Matrix current_tf = parent_tf * Matrix::from_vec(s.tf);
        bool is_plane = s.tags.contains("is_plane") && s.tags["is_plane"].get<bool>();
        bool is_pwh = s.tags.contains("is_pwh") && s.tags["is_pwh"].get<bool>();
        
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            bool is_vol = !geo.faces.empty() && !is_plane && !is_pwh; 
            tool_nodes.push_back({geo, current_tf, is_vol, is_plane, is_pwh});
        } else if (is_plane) {
            tool_nodes.push_back({Geometry(), current_tf, false, true, false});
        }

        for (const auto& child : s.components) {
            collect_tool_geometry(vfs, child, current_tf, tool_nodes);
        }
    }

    static void recursive_subtract(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<ToolNode>& tool_nodes, bool open) {
        Matrix subject_world_tf = parent_tf * Matrix::from_vec(s.tf);
        Matrix subject_world_inv = subject_world_tf.inverse();

        if (s.geometry.has_value()) {
            Geometry target_geo = vfs->read<Geometry>(s.geometry.value());
            bool has_faces = !target_geo.faces.empty();
            bool has_segments = !target_geo.segments.empty();
            bool has_only_points = target_geo.faces.empty() && target_geo.segments.empty() && !target_geo.vertices.empty();

            if (has_faces) {
                auto target_plane_opt = target_geo.find_plane();
                bool is_target_pwh = target_plane_opt.has_value();

                if (is_target_pwh) {
                    // Try PWH-to-PWH coplanar path first
                    EK::Plane_3 target_plane = *target_plane_opt;
                    Matrix project_tf = Matrix::lookAt(target_plane.point(), target_plane.orthogonal_vector());
                    Matrix rehydrate_tf = project_tf.inverse();

                    boolean::General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, project_tf, subject_set);
                    
                    bool used_pwh_path = false;
                    for (const auto& tool : tool_nodes) {
                        Geometry local_tool_geo = tool.geo;
                        Matrix tool_rel_tf = subject_world_inv * tool.world_tf;
                        local_tool_geo.apply_tf(tool_rel_tf.to_vec());

                        if (local_tool_geo.is_coplanar_with(target_plane)) {
                            boolean::General_polygon_set_2 tool_set;
                            add_geometry_to_gps(local_tool_geo, project_tf, tool_set);
                            boolean::Engine::cut_gps_by_gps(subject_set, tool_set);
                            used_pwh_path = true;
                        } else if (!tool.is_plane) {
                            // Non-coplanar tool: Use 3D mesh clipping on the projected surface?
                            // For now, fall back to 3D mesh processing for the whole target.
                            used_pwh_path = false; break;
                        }
                    }

                    if (used_pwh_path) {
                        target_geo = gps_to_geometry(subject_set);
                        target_geo.apply_tf(rehydrate_tf.to_vec());
                    } else {
                        is_target_pwh = false; // Fall through to 3D
                    }
                }

                if (!is_target_pwh) {
                    boolean::Surface_mesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        if (tool.is_plane) {
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            EK::Plane_3 local_plane = rel_tf.transform(EK::Plane_3(0, 0, 1, 0));
                            boolean::Engine::cut_mesh_by_plane(target_mesh, local_plane);
                        } else {
                            boolean::Surface_mesh tool_mesh = geometry_to_mesh(tool.geo);
                            Matrix rel_tf = subject_world_inv * tool.world_tf;
                            transform_mesh(tool_mesh, rel_tf);
                            if (tool.is_volume) assert(fix::is_geometry_solid(tool_mesh));
                            boolean::Engine::cut_mesh_by_mesh(target_mesh, tool_mesh);
                        }
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                }
            } else if (has_segments) {
                // ... (previous segment logic)
            } else if (has_only_points) {
                // ... (previous point logic)
            }
            s.geometry = vfs->materialize<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_subtract(vfs, child, subject_world_tf, tool_nodes, open);
        }
    }

    // (Helper methods unchanged)
    static boolean::Surface_mesh geometry_to_mesh(const Geometry& geo) {
        std::vector<EK::Point_3> pts;
        std::vector<std::vector<std::size_t>> faces;
        for (const auto& v : geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
        for (const auto& f : geo.faces) {
            if (f.loops.empty()) continue;
            std::vector<std::size_t> face;
            for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
            faces.push_back(face);
        }
        CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
        CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
        boolean::Surface_mesh mesh;
        std::vector<boolean::Surface_mesh::Vertex_index> v_indices;
        for (const auto& p : pts) v_indices.push_back(mesh.add_vertex(p));
        for (const auto& f : faces) {
            std::vector<boolean::Surface_mesh::Vertex_index> face_vs;
            for (auto idx : f) face_vs.push_back(v_indices[idx]);
            mesh.add_face(face_vs);
        }
        CGAL::Polygon_mesh_processing::triangulate_faces(mesh);
        return mesh;
    }

    static Geometry mesh_to_geometry(const boolean::Surface_mesh& mesh) {
        Geometry geo;
        std::map<boolean::Surface_mesh::Vertex_index, int> v_map;
        for (auto v : mesh.vertices()) {
            v_map[v] = (int)geo.vertices.size();
            auto p = mesh.point(v);
            geo.vertices.push_back({p.x(), p.y(), p.z()});
        }
        for (auto f : mesh.faces()) {
            Geometry::Face face;
            std::vector<int> loop;
            for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) loop.push_back(v_map[v]);
            face.loops.push_back(loop);
            geo.faces.push_back(face);
        }
        return geo;
    }

    static void transform_mesh(boolean::Surface_mesh& mesh, const Matrix& tf) {
        for (auto v : mesh.vertices()) mesh.point(v) = tf.transform(mesh.point(v));
    }

    static void add_geometry_to_gps(const Geometry& geo, const Matrix& tf, boolean::General_polygon_set_2& gps) {
        Geometry t = geo;
        t.apply_tf(tf.to_vec());
        for (const auto& face : t.faces) {
            if (face.loops.empty()) continue;
            boolean::Polygon_2 boundary;
            for (int idx : face.loops[0]) boundary.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
            
            try {
                if (boundary.is_empty() || !boundary.is_simple()) continue;
                if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();
                
                std::vector<boolean::Polygon_2> holes;
                for (size_t i = 1; i < face.loops.size(); ++i) {
                    boolean::Polygon_2 h;
                    for (int idx : face.loops[i]) h.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
                    if (h.is_simple()) {
                        if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                        holes.push_back(h);
                    }
                }
                gps.join(boolean::Polygon_with_holes_2(boundary, holes.begin(), holes.end()));
            } catch (...) {
                // Skip invalid polygons gracefully
            }
        }
    }

    static Geometry gps_to_geometry(const boolean::General_polygon_set_2& gps) {
        Geometry g;
        std::vector<boolean::Polygon_with_holes_2> pwhs;
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

    static std::vector<std::string> argument_keys() { return {"$in", "tools", "open"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/cut"}, 
            {"arguments", { 
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}, 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"affiliate", "$out"}}, 
                {{"name", "open"}, {"type", "jot:boolean"}, {"default", false}} 
            }}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void cut_init(fs::VFSNode* vfs) {
    Processor::register_op<CutOp<>, Shape, std::vector<Shape>, bool>(vfs, "jot/cut");
}

} // namespace geo
} // namespace jotcad
