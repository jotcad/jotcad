#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/convex_hull_3.h>
#include <CGAL/Nef_polyhedron_3.h>
#include <CGAL/minkowski_sum_3.h>
#include <CGAL/boost/graph/convert_nef_polyhedron_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>


namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GrowOp : P {
    static constexpr const char* path = "jot/grow";

    static void collect_points(fs::VFSNode* vfs, const Shape& s, const Matrix& parent_tf, std::vector<EK::Point_3>& pts) {
        Matrix current_tf = parent_tf * s.tf;
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            for (const auto& v : geo.vertices) pts.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
        }
        for (const auto& child : s.components) collect_points(vfs, child, current_tf, pts);
    }

    static Geometry grow_cloud(const std::vector<EK::Point_3>& cloud, const std::vector<EK::Point_3>& tool_pts) {
        std::vector<EK::Point_3> sum_pts;
        for (const auto& p : cloud) {
            for (const auto& tp : tool_pts) {
                sum_pts.push_back(EK::Point_3(p.x() + tp.x(), p.y() + tp.y(), p.z() + tp.z()));
            }
        }

        if (sum_pts.size() < 3) return Geometry();

        // Planarity Check
        bool is_flat = false;
        EK::Plane_3 plane;
        bool found_plane = false;
        for (size_t i = 2; i < sum_pts.size(); ++i) {
            EK::Plane_3 p(sum_pts[0], sum_pts[1], sum_pts[i]);
            if (!p.is_degenerate()) {
                plane = p;
                found_plane = true;
                break;
            }
        }
        if (found_plane) {
            is_flat = true;
            for (const auto& p : sum_pts) {
                if (!plane.has_on(p)) { is_flat = false; break; }
            }
        } else {
            is_flat = true;
        }

        boolean::Surface_mesh hull;
        CGAL::convex_hull_3(sum_pts.begin(), sum_pts.end(), hull);

        if (is_flat) {
            Geometry res_geo;
            std::map<boolean::ExactMesh::Vertex_index, int> v_map;
            for (auto v : hull.vertices()) {
                v_map[v] = (int)res_geo.vertices.size();
                auto p = hull.point(v);
                res_geo.vertices.push_back({p.x(), p.y(), p.z()});
            }
            for (auto f : hull.faces()) {
                Geometry::Face face;
                std::vector<int> loop;
                for (auto v : hull.vertices_around_face(hull.halfedge(f))) loop.push_back(v_map[v]);
                face.loops.push_back(loop);
                res_geo.faces.push_back(face);
            }
            return res_geo;
        } else {
            return boolean::Engine::mesh_to_geometry(hull);
        }
    }

    static void execute_decomposed(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<EK::Point_3>& tool_pts, const Shape& tool_shape) {
        if (!s.geometry.has_value()) return;
        Matrix current_tf = parent_tf * s.tf;
        Matrix inv_tf = current_tf.inverse();
        Geometry subject_geo = vfs->read<Geometry>(s.geometry.value());

        // 1. Solids and Surfaces: Global Growth
        std::string type = s.tags.value("type", "");
        if ((type == "closed" || type == "surface" || type == "open") && !subject_geo.vertices.empty()) {
            boolean::Surface_mesh subject_mesh = boolean::Engine::geometry_to_mesh(subject_geo);
            
            // Check if shape is convex to determine path
            if (CGAL::is_strongly_convex_3(subject_mesh)) {
                // Fast Path: Convex Hull of summed points
                std::vector<EK::Point_3> cloud;
                for (const auto& v : subject_geo.vertices) {
                    cloud.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
                }
                Geometry res = grow_cloud(cloud, tool_pts);
                if (res.vertices.empty()) return;
                res.apply_tf(inv_tf);
                res.triangulate();
                s.geometry = vfs->materialize(res);
                return;
            } else {
                // Exact Path: Nef Polyhedron Minkowski Sum
                try {
                    // Bring subject to local coordinates of current_tf
                    boolean::Engine::transform_mesh(subject_mesh, current_tf);
                    CGAL::Nef_polyhedron_3<EK> nef_subject(subject_mesh);
                    
                    boolean::Surface_mesh tool_mesh;
                    if (tool_shape.geometry.has_value()) {
                        Geometry tool_geo = vfs->read<Geometry>(tool_shape.geometry.value());
                        tool_geo.apply_tf(tool_shape.tf);
                        tool_mesh = boolean::Engine::geometry_to_mesh(tool_geo);
                    } else {
                        // Fallback to convex hull of tool points
                        CGAL::convex_hull_3(tool_pts.begin(), tool_pts.end(), tool_mesh);
                    }
                    
                    CGAL::Nef_polyhedron_3<EK> nef_tool(tool_mesh);
                    CGAL::Nef_polyhedron_3<EK> nef_res = CGAL::minkowski_sum_3(nef_subject, nef_tool);
                    
                    boolean::Surface_mesh res_mesh;
                    CGAL::convert_nef_polyhedron_to_polygon_mesh(nef_res, res_mesh);
                    
                    // Triangulate faces to satisfy the triangle mesh requirement in mesh_to_geometry
                    CGAL::Polygon_mesh_processing::triangulate_faces(res_mesh);
                    
                    // Transform back to local coordinates
                    boolean::Engine::transform_mesh(res_mesh, inv_tf);
                    
                    Geometry res = boolean::Engine::mesh_to_geometry(res_mesh);
                    if (!res.vertices.empty()) {
                        res.triangulate();
                        s.geometry = vfs->materialize(res);
                        return;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[GrowOp] Nef Minkowski sum failed: " << e.what() << ", falling back to convex hull" << std::endl;
                }
                
                // Fallback to convex hull if Nef fails
                std::vector<EK::Point_3> cloud;
                for (const auto& v : subject_geo.vertices) {
                    cloud.push_back(current_tf.transform(EK::Point_3(v.x, v.y, v.z)));
                }
                Geometry res = grow_cloud(cloud, tool_pts);
                if (res.vertices.empty()) return;
                res.apply_tf(inv_tf);
                res.triangulate();
                s.geometry = vfs->materialize(res);
                return;
            }
        }

        // 2. Segments: Partitioned Growth
        if (subject_geo.segments.empty()) return; // Loose vertices: Ignore

        std::vector<Geometry> grown_geos;
        for (const auto& seg : subject_geo.segments) {
            std::vector<EK::Point_3> cloud = {
                current_tf.transform(EK::Point_3(subject_geo.vertices[seg[0]].x, subject_geo.vertices[seg[0]].y, subject_geo.vertices[seg[0]].z)),
                current_tf.transform(EK::Point_3(subject_geo.vertices[seg[1]].x, subject_geo.vertices[seg[1]].y, subject_geo.vertices[seg[1]].z))
            };
            Geometry g = grow_cloud(cloud, tool_pts);
            if (!g.vertices.empty()) grown_geos.push_back(g);
        }

        if (grown_geos.empty()) return;

        // Union the partitioned segment-hulls
        Shape union_container;
        for (auto& g : grown_geos) {
            Shape child;
            g.triangulate();
            child.geometry = vfs->materialize(g);
            child.tags["type"] = g.is_plane() ? "surface" : "closed";
            union_container.components.push_back(child);
        }

        if (union_container.components.size() == 1) {
            Geometry final_geo = vfs->read<Geometry>(union_container.components[0].geometry.value());
            final_geo.apply_tf(inv_tf);
            final_geo.triangulate();
            s.geometry = vfs->materialize(final_geo);
            s.tags["type"] = union_container.components[0].tags["type"];
            return;
        }

        Shape target = union_container.components[0];
        std::vector<boolean::Engine::ToolNode> tools;
        for (size_t i = 1; i < union_container.components.size(); ++i) {
            boolean::Engine::collect_tool_geometry(vfs, union_container.components[i], Matrix::identity(), tools);
        }

        boolean::Engine::recursive_union(vfs, target, Matrix::identity(), tools);
        
        Geometry final_geo = vfs->read<Geometry>(target.geometry.value());
        final_geo.apply_tf(inv_tf);
        final_geo.triangulate();
        s.geometry = vfs->materialize(final_geo);
        s.tags["type"] = target.tags["type"];
    }

    static void process_shape_recursive(fs::VFSNode* vfs, Shape& s, const Matrix& parent_tf, const std::vector<EK::Point_3>& tool_pts, const Shape& tool_shape) {
        execute_decomposed(vfs, s, parent_tf, tool_pts, tool_shape);
        for (auto& child : s.components) process_shape_recursive(vfs, child, parent_tf * s.tf, tool_pts, tool_shape);
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& tool_shape) {
        if (!in.geometry.has_value() && in.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        std::vector<EK::Point_3> tool_pts;
        collect_points(vfs, tool_shape, Matrix::identity(), tool_pts);
        if (tool_pts.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Shape out = in;
        process_shape_recursive(vfs, out, Matrix::identity(), tool_pts, tool_shape);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tool"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/grow"},
            {"description", "Grows the subject geometry by sweeping a tool shape over it."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to grow."}}}}},
            {"arguments", {
                {{"name", "tool"}, {"type", "jot:shape"}}
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
