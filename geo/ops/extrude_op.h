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
    static void execute_sweep(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Matrix& bottom_tf, const Matrix& top_tf) {
        if (!in.is_solid() && !in.is_gap()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;

        // 1. Process Faces -> Mesh
        if (!geo.faces.empty() || !geo.triangles.empty()) {
            boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
            if (!mesh.is_empty()) {
                boolean::Surface_mesh extruded_mesh;
                typedef typename boost::property_map<boolean::Surface_mesh, CGAL::vertex_point_t>::type VPMap;
                
                ExtrudeProject<VPMap> bottom_proj(get(CGAL::vertex_point, extruded_mesh), bottom_tf);
                ExtrudeProject<VPMap> top_proj(get(CGAL::vertex_point, extruded_mesh), top_tf);
                
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
            
            Point_3 s_bot = bottom_tf.transform(s);
            Point_3 t_bot = bottom_tf.transform(t);
            Point_3 s_top = top_tf.transform(s);
            Point_3 t_top = top_tf.transform(t);
            
            int v0 = (int)res.vertices.size();
            res.vertices.push_back({s_bot.x(), s_bot.y(), s_bot.z()});
            res.vertices.push_back({t_bot.x(), t_bot.y(), t_bot.z()});
            res.vertices.push_back({t_top.x(), t_top.y(), t_top.z()});
            res.vertices.push_back({s_top.x(), s_top.y(), s_top.z()});
            
            res.faces.push_back({{{v0, v0+1, v0+2, v0+3}}});
        }

        // 3. Process Points -> Segments
        for (int p_idx : geo.points) {
            Point_3 p(geo.vertices[p_idx].x, geo.vertices[p_idx].y, geo.vertices[p_idx].z);
            Point_3 p_bot = bottom_tf.transform(p);
            Point_3 p_top = top_tf.transform(p);
            
            int v0 = (int)res.vertices.size();
            res.vertices.push_back({p_bot.x(), p_bot.y(), p_bot.z()});
            res.vertices.push_back({p_top.x(), p_top.y(), p_top.z()});
            res.segments.push_back({v0, v0+1});
        }

        if (res.vertices.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        Shape out = in;
        std::string in_type = in.tags.value("type", "");
        if (in_type == "points") out.add_tag("type", "segments");
        else if (in_type == "segments") out.add_tag("type", "open");
        else if (in_type == "surface") out.add_tag("type", "closed");
        else if (in_type == "open") out.add_tag("type", "closed"); // Extruding a shell might close it, or keep it open. Default to closed if possible? Actually segments->open is safer.
        
        out.geometry = vfs->materialize<Geometry>(res);
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrude";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& target, std::optional<Interval> range) {
        if (target.is_number() || target.is_array()) {
            // Case 1: Normal-based extrusion (target is distance/interval)
            if (!in.geometry.has_value()) {
                vfs->write(fulfilling.with_output("$out"), in);
                return;
            }
            Geometry geo = vfs->read<Geometry>(in.geometry.value());
            auto p_opt = geo.find_plane();
            
            Interval iv = Interval::from_json(target);

            if (!p_opt) {
                // Fallback to local Z if no plane found
                ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, 
                    Matrix::translate(0, 0, FT(iv.min)), 
                    Matrix::translate(0, 0, FT(iv.max)));
                return;
            }

            // Normal vector in local space
            Vector_3 n = p_opt->orthogonal_vector();
            Matrix norm_frame = Matrix::fromNormal(Point_3(0,0,0), n);
            Matrix b_tf = norm_frame * Matrix::translate(0, 0, FT(iv.min)) * norm_frame.inverse();
            Matrix t_tf = norm_frame * Matrix::translate(0, 0, FT(iv.max)) * norm_frame.inverse();

            ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, b_tf, t_tf);
        } else {
            // Case 2: Reference-based extrusion (target is a Shape defining an axis)
            Shape target_shape = Processor::decode<Shape>(vfs, "target", {{"target", target}}, schema(), {});
            
            // The relative transform to the target shape's coordinate system
            Matrix to_target = in.tf.inverse() * target_shape.tf;

            if (range.has_value()) {
                // Extrude between range.min and range.max along the target's local Z axis
                Matrix b_tf = to_target * Matrix::translate(0, 0, FT(range->min));
                Matrix t_tf = to_target * Matrix::translate(0, 0, FT(range->max));
                ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, b_tf, t_tf);
            } else {
                // Default: Extrude from current position (identity) to the target's origin
                ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, Matrix::identity(), to_target);
            }
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "target", "range"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrude"},
            {"description", "Extrudes geometry. If target is an interval/number, extrudes along the face normal. If target is a shape, uses it as a reference plane/axis."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "target"}, {"type", "jot:any"}, {"description", "Interval/Number (length) or Shape (reference frame)"}},
                {{"name", "range"}, {"type", "jot:interval"}, {"optional", true}, {"description", "Optional start/end offsets along the reference axis"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeXOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, Interval height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, 
            Matrix::translate(FT(height.min), 0, 0), 
            Matrix::translate(FT(height.max), 0, 0));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeX"},
            {"description", "Extrudes geometry along the local X axis."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeYOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, Interval height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, 
            Matrix::translate(0, FT(height.min), 0), 
            Matrix::translate(0, FT(height.max), 0));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeY"},
            {"description", "Extrudes geometry along the local Y axis."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ExtrudeZOp : ExtrudeOpBase<P> {
    static constexpr const char* path = "jot/extrudeZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, Interval height) {
        ExtrudeOpBase<P>::execute_sweep(vfs, fulfilling, in, 
            Matrix::translate(0, 0, FT(height.min)), 
            Matrix::translate(0, 0, FT(height.max)));
    }
    static std::vector<std::string> argument_keys() { return {"$in", "height"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/extrudeZ"},
            {"description", "Extrudes geometry along the local Z axis."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "height"}, {"type", "jot:interval"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void extrude_init(fs::VFSNode* vfs) {
    Processor::register_op<ExtrudeOp<>, Shape, nlohmann::json, std::optional<Interval>>(vfs, "jot/extrude");
    Processor::register_op<ExtrudeOp<>, Shape, nlohmann::json, std::optional<Interval>>(vfs, "jot/e");
    Processor::register_op<ExtrudeXOp<>, Shape, Interval>(vfs, "jot/extrudeX");
    Processor::register_op<ExtrudeXOp<>, Shape, Interval>(vfs, "jot/ex");
    Processor::register_op<ExtrudeYOp<>, Shape, Interval>(vfs, "jot/extrudeY");
    Processor::register_op<ExtrudeYOp<>, Shape, Interval>(vfs, "jot/ey");
    Processor::register_op<ExtrudeZOp<>, Shape, Interval>(vfs, "jot/extrudeZ");
    Processor::register_op<ExtrudeZOp<>, Shape, Interval>(vfs, "jot/ez");
}

} // namespace geo
} // namespace jotcad
