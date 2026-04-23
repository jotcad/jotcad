#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"
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
    };

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools, bool open = false) {
        Shape out = in;
        if (tools.empty()) {
            vfs->write<Shape>(fulfilling, out);
            return;
        }

        std::vector<ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        recursive_subtract(vfs, out, Matrix::identity(), tool_nodes, open);
        vfs->write<Shape>(fulfilling, out, "$out");
    }

    static void collect_tool_geometry(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<ToolNode>& tool_nodes) {
        Matrix current_tf = parent_tf * Matrix::from_vec(s.tf);
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            bool is_vol = !geo.faces.empty(); 
            tool_nodes.push_back({geo, current_tf, is_vol});
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
            bool has_only_points = target_geo.faces.empty() && !target_geo.vertices.empty();

            if (has_faces) {
                bool is_3d = false;
                for (const auto& v : target_geo.vertices) {
                    if (CGAL::to_double(CGAL::abs(v.z)) > 1e-6) { is_3d = true; break; }
                }

                if (is_3d) {
                    boolean::Surface_mesh target_mesh = geometry_to_mesh(target_geo);
                    for (const auto& tool : tool_nodes) {
                        boolean::Surface_mesh tool_mesh = geometry_to_mesh(tool.geo);
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        transform_mesh(tool_mesh, rel_tf);
                        boolean::Engine::cut_mesh_by_mesh(target_mesh, tool_mesh);
                    }
                    target_geo = mesh_to_geometry(target_mesh);
                } else {
                    boolean::General_polygon_set_2 subject_set;
                    add_geometry_to_gps(target_geo, Matrix::identity(), subject_set);
                    for (const auto& tool : tool_nodes) {
                        Matrix rel_tf = subject_world_inv * tool.world_tf;
                        boolean::General_polygon_set_2 tool_set;
                        add_geometry_to_gps(tool.geo, rel_tf, tool_set);
                        boolean::Engine::cut_gps_by_gps(subject_set, tool_set);
                    }
                    target_geo = gps_to_geometry(subject_set);
                }
            } else if (has_only_points) {
                std::cout << "[CUT DEBUG] Target has " << target_geo.vertices.size() << " points." << std::endl;
                std::vector<EK::Point_3> pts;
                for (const auto& v : target_geo.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
                
                for (const auto& tool : tool_nodes) {
                    boolean::Surface_mesh tool_mesh = geometry_to_mesh(tool.geo);
                    Matrix rel_tf = subject_world_inv * tool.world_tf;
                    transform_mesh(tool_mesh, rel_tf);
                    std::cout << "[CUT DEBUG] Applying Tool Mesh. Closed: " << CGAL::is_closed(tool_mesh) << std::endl;
                    boolean::Engine::cut_points_by_mesh(pts, tool_mesh);
                }
                
                std::cout << "[CUT DEBUG] Points remaining after cut: " << pts.size() << std::endl;
                target_geo.vertices.clear();
                for (const auto& p : pts) target_geo.vertices.push_back({p.x(), p.y(), p.z()});
            }

            s.geometry = vfs->write_anonymous<Geometry>(target_geo);
        }

        for (auto& child : s.components) {
            recursive_subtract(vfs, child, subject_world_tf, tool_nodes, open);
        }
    }

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
        for (auto v : mesh.vertices()) {
            mesh.point(v) = tf.transform(mesh.point(v));
        }
    }

    static void add_geometry_to_gps(const Geometry& geo, const Matrix& tf, boolean::General_polygon_set_2& gps) {
        Geometry t = geo;
        t.apply_tf(tf.to_vec());
        for (const auto& face : t.faces) {
            if (face.loops.empty()) continue;
            boolean::Polygon_2 boundary;
            for (int idx : face.loops[0]) boundary.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
            if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();
            if (!boundary.is_simple()) continue;

            std::vector<boolean::Polygon_2> holes;
            for (size_t i = 1; i < face.loops.size(); ++i) {
                boolean::Polygon_2 h;
                for (int idx : face.loops[i]) h.push_back(EK::Point_2(t.vertices[idx].x, t.vertices[idx].y));
                if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                if (h.is_simple()) holes.push_back(h);
            }
            try { gps.join(boolean::Polygon_with_holes_2(boundary, holes.begin(), holes.end())); } catch (...) {}
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
                {"$in", {{"type", "jot:shape"}}},
                {"tools", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}},
                {"open", {{"type", "boolean"}, {"default", false}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void cut_init() {
    Processor::register_op<CutOp<>, Shape, std::vector<Shape>, bool>("jot/cut");
}

} // namespace geo
} // namespace jotcad
