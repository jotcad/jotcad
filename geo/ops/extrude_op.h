#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/extrude.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>

namespace jotcad {
namespace geo {

template <typename MAP>
struct ExtrudeProject {
    ExtrudeProject(MAP map, const Matrix& tf) : map(map), tf(tf) {}
    template <typename VD, typename T>
    void operator()(const T&, VD vd) const {
        put(map, vd, tf.transform(get(map, vd)));
    }
    MAP map;
    Matrix tf;
};

template <typename P = JotVfsProtocol>
struct ExtrudeOpBase : P {
    static void execute_sweep(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Matrix& relative_tf) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;

        // 1. Process Faces -> Mesh
        if (!geo.faces.empty()) {
            boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
            if (!mesh.is_empty()) {
                boolean::Surface_mesh extruded_mesh;
                typedef typename boost::property_map<boolean::Surface_mesh, CGAL::vertex_point_t>::type VPMap;
                
                ExtrudeProject<VPMap> bottom_proj(get(CGAL::vertex_point, extruded_mesh), Matrix::identity());
                ExtrudeProject<VPMap> top_proj(get(CGAL::vertex_point, extruded_mesh), relative_tf);
                
                CGAL::Polygon_mesh_processing::extrude_mesh(mesh, extruded_mesh, bottom_proj, top_proj);
                CGAL::Polygon_mesh_processing::triangulate_faces(extruded_mesh);
                
                if (CGAL::is_closed(extruded_mesh)) {
                    FT vol = CGAL::Polygon_mesh_processing::volume(extruded_mesh);
                    if (vol < 0) {
                        CGAL::Polygon_mesh_processing::reverse_face_orientations(extruded_mesh);
                    }
                }
                res = boolean::Engine::mesh_to_geometry(extruded_mesh);
            }
        }

        // 2. Process Segments -> Quads
        for (const auto& seg : geo.segments) {
            Point_3 s(geo.vertices[seg[0]].x, geo.vertices[seg[0]].y, geo.vertices[seg[0]].z);
            Point_3 t(geo.vertices[seg[1]].x, geo.vertices[seg[1]].y, geo.vertices[seg[1]].z);
            
            Point_3 s_top = relative_tf.transform(s);
            Point_3 t_top = relative_tf.transform(t);
            
            int v0 = (int)res.vertices.size();
            res.vertices.push_back({s.x(), s.y(), s.z()});
            res.vertices.push_back({t.x(), t.y(), t.z()});
            res.vertices.push_back({t_top.x(), t_top.y(), t_top.z()});
            res.vertices.push_back({s_top.x(), s_top.y(), s_top.z()});
            
            res.faces.push_back({{{v0, v0+1, v0+2, v0+3}}});
        }

        // 3. Process Points -> Segments
        for (int p_idx : geo.points) {
            Point_3 p(geo.vertices[p_idx].x, geo.vertices[p_idx].y, geo.vertices[p_idx].z);
            Point_3 p_top = relative_tf.transform(p);
            
            int v0 = (int)res.vertices.size();
            res.vertices.push_back({p.x(), p.y(), p.z()});
            res.vertices.push_back({p_top.x(), p_top.y(), p_top.z()});
            res.segments.push_back({v0, v0+1});
        }

        if (res.vertices.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Shape out = in;
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrude";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& target) {
        if (target.is_number()) {
            // Normal Sweep: Extrude along the subject's face normal by the given value.
            if (!in.geometry.has_value()) {
                vfs->write(fulfilling.with_output("$out"), in);
                return;
            }
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            auto p_opt = geo.find_plane();
            
            double d_val = target.template get<double>();

            if (!p_opt) {
                // Fallback to Z if no plane found
                ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, Matrix::translate(0, 0, FT(d_val)));
                return;
            }

            // Normal vector in local space
            Vector_3 n = p_opt->orthogonal_vector();
            Matrix norm_frame = Matrix::fromNormal(Point_3(0,0,0), n);
            Matrix move = norm_frame * Matrix::translate(0, 0, FT(d_val)) * norm_frame.inverse();

            ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, move);
        } else {
            // Transform Sweep: Extrude to a target frame.
            Shape target_shape = Processor::decode<Shape>(vfs, "target", {{"target", target}}, schema(), {});
            Matrix rel = in.tf.inverse() * target_shape.tf;
            ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, rel);
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "target"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrude"},
            {"description", "Extrudes geometry. If target is a number, extrudes along the face normal. If target is a shape, performs a transform sweep."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "target"}, {"type", "jot:any"}, {"description", "Number (length) or Shape (destination frame)"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeXOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, Matrix::translate(FT(height), 0, 0));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeX"},
            {"description", "Extrudes geometry along the local X axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 1.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeYOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, Matrix::translate(0, FT(height), 0));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeY"},
            {"description", "Extrudes geometry along the local Y axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 1.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeZOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, Matrix::translate(0, 0, FT(height)));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeZ"},
            {"description", "Extrudes geometry along the local Z axis."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "height"}, {"type", "jot:number"}, {"default", 1.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void extrude_init(fs::VFSNode* vfs) {
    Processor::register_op<ExtrudeOp<>, Shape, Shape>(vfs, "jot/extrude");
    Processor::register_op<ExtrudeOp<>, Shape, Shape>(vfs, "jot/e");
    Processor::register_op<ExtrudeXOp<>, Shape, double>(vfs, "jot/extrudeX");
    Processor::register_op<ExtrudeXOp<>, Shape, double>(vfs, "jot/ex");
    Processor::register_op<ExtrudeYOp<>, Shape, double>(vfs, "jot/extrudeY");
    Processor::register_op<ExtrudeYOp<>, Shape, double>(vfs, "jot/ey");
    Processor::register_op<ExtrudeZOp<>, Shape, double>(vfs, "jot/extrudeZ");
    Processor::register_op<ExtrudeZOp<>, Shape, double>(vfs, "jot/ez");
}

} // namespace geo
} // namespace jotcad
