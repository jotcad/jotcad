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

    static void collect_points(fs::VFSNode* vfs, const Shape& s, std::vector<EK::Point_3>& pts) {
        for (const auto& node : s.shapes()) {
            if (!node.has_positive_geometry()) continue;
            Geometry geo = vfs->read<Geometry>(node.geometry.value());
            for (const auto& v : geo.vertices) {
                pts.push_back(node.tf.transform(EK::Point_3(v.x, v.y, v.z)));
            }
        }
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

    static void execute_decomposed(fs::VFSNode* vfs, Shape& s, const std::vector<EK::Point_3>& tool_pts, const Shape& tool_shape) {
        if (!s.geometry.has_value() || !s.has_positive_geometry()) return;
        Matrix inv_tf = s.tf.inverse();
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
                    cloud.push_back(s.tf.transform(EK::Point_3(v.x, v.y, v.z)));
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
                    // Bring subject to local coordinates of s.tf
                    boolean::Engine::transform_mesh(subject_mesh, s.tf);
                    CGAL::Nef_polyhedron_3<EK> subject_nef(subject_mesh);

                    // Reconstruct tool as Nef Polyhedron
                    boolean::Surface_mesh tool_mesh;
                    if (tool_shape.geometry.has_value() && tool_shape.has_positive_geometry()) {
                        Geometry tool_geo = vfs->read<Geometry>(tool_shape.geometry.value());
                        tool_geo.apply_tf(tool_shape.tf);
                        tool_mesh = boolean::Engine::geometry_to_mesh(tool_geo);
                    } else {
                        std::vector<EK::Point_3> local_tool_pts;
                        collect_points(vfs, tool_shape, local_tool_pts);
                        CGAL::convex_hull_3(local_tool_pts.begin(), local_tool_pts.end(), tool_mesh);
                    }
                    CGAL::Nef_polyhedron_3<EK> tool_nef(tool_mesh);

                    // Minkowski sum
                    CGAL::Nef_polyhedron_3<EK> sum_nef = CGAL::minkowski_sum_3(subject_nef, tool_nef);

                    if (sum_nef.is_simple()) {
                        boolean::Surface_mesh res_mesh;
                        CGAL::convert_nef_polyhedron_to_polygon_mesh(sum_nef, res_mesh);
                        CGAL::Polygon_mesh_processing::triangulate_faces(res_mesh);
                        Geometry res = boolean::Engine::mesh_to_geometry(res_mesh);
                        res.apply_tf(inv_tf);
                        res.triangulate();
                        s.geometry = vfs->materialize(res);
                        return;
                    }
                } catch (...) {
                    // Fallback to decomposition
                }
            }
        }

        // 2. Decomposed Growth: Elements (faces/segments/points) grown individually and unioned
        Shape union_container;
        union_container.tf = Matrix::identity();
        union_container.tags["type"] = "group";

        // A. Faces
        for (const auto& f : subject_geo.faces) {
            std::vector<EK::Point_3> f_pts;
            for (const auto& l : f.loops) {
                for (int idx : l) f_pts.push_back(s.tf.transform(EK::Point_3(subject_geo.vertices[idx].x, subject_geo.vertices[idx].y, subject_geo.vertices[idx].z)));
            }
            Geometry g = grow_cloud(f_pts, tool_pts);
            if (g.vertices.empty()) continue;
            Shape child;
            child.geometry = vfs->materialize(g);
            child.tags["type"] = g.is_plane() ? "surface" : "closed";
            union_container.components.push_back(child);
        }

        // B. Segments
        for (const auto& seg : subject_geo.segments) {
            std::vector<EK::Point_3> seg_pts = {
                s.tf.transform(EK::Point_3(subject_geo.vertices[seg[0]].x, subject_geo.vertices[seg[0]].y, subject_geo.vertices[seg[0]].z)),
                s.tf.transform(EK::Point_3(subject_geo.vertices[seg[1]].x, subject_geo.vertices[seg[1]].y, subject_geo.vertices[seg[1]].z))
            };
            Geometry g = grow_cloud(seg_pts, tool_pts);
            if (g.vertices.empty()) continue;
            Shape child;
            child.geometry = vfs->materialize(g);
            child.tags["type"] = g.is_plane() ? "surface" : "closed";
            union_container.components.push_back(child);
        }

        // C. Points
        for (int p_idx : subject_geo.points) {
            std::vector<EK::Point_3> p_pts = {
                s.tf.transform(EK::Point_3(subject_geo.vertices[p_idx].x, subject_geo.vertices[p_idx].y, subject_geo.vertices[p_idx].z))
            };
            Geometry g = grow_cloud(p_pts, tool_pts);
            if (g.vertices.empty()) continue;
            Shape child;
            child.geometry = vfs->materialize(g);
            child.tags["type"] = g.is_plane() ? "surface" : "closed";
            union_container.components.push_back(child);
        }

        if (union_container.components.empty()) return;

        // D. Fallback if no elements were explicitly defined
        if (subject_geo.faces.empty() && subject_geo.segments.empty() && subject_geo.points.empty() && !subject_geo.vertices.empty()) {
            std::vector<EK::Point_3> v_pts;
            for (const auto& v : subject_geo.vertices) {
                v_pts.push_back(s.tf.transform(EK::Point_3(v.x, v.y, v.z)));
            }
            Geometry g = grow_cloud(v_pts, tool_pts);
            if (g.vertices.empty()) return;
            Shape child;
            child.geometry = vfs->materialize(g);
            child.tags["type"] = g.is_plane() ? "surface" : "closed";
            union_container.components.push_back(child);
        }

        Shape target = union_container.components[0];
        std::vector<boolean::Engine::ToolNode> tools;
        for (size_t i = 1; i < union_container.components.size(); ++i) {
            boolean::Engine::collect_tool_geometry(vfs, union_container.components[i], Matrix::identity(), tools);
        }

        boolean::Engine::recursive_union(vfs, target, tools);
        
        Geometry final_geo = vfs->read<Geometry>(target.geometry.value());
        final_geo.apply_tf(inv_tf);
        final_geo.triangulate();
        s.geometry = vfs->materialize(final_geo);
        s.tags["type"] = target.tags["type"];
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& tool_shape) {
        if (!in.geometry.has_value() && in.components.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        std::vector<EK::Point_3> tool_pts;
        collect_points(vfs, tool_shape, tool_pts);
        if (tool_pts.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Shape out = in.map([&](Shape node) {
            if (node.has_positive_geometry()) {
                execute_decomposed(vfs, node, tool_pts, tool_shape);
            }
            return node;
        });
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
