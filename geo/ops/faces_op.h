#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FacesOp : P {
    static constexpr const char* path = "jot/faces";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res;
        res.vertices = geo.vertices;
        res.faces = geo.faces;
        
        // Zero-Base Tags for the group
        Shape out;
        out.tf = in.tf;
        out.geometry = vfs->materialize<Geometry>(res);
        out.add_tag("type", "faces");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/faces"},
            {"description", "Extracts faces from a shape as a mesh."},
            {"arguments", {{{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct EachFaceOp : P {
    static constexpr const char* path = "jot/eachFace";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, bool proxy = true) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        
        // Zero-Base Tags for the group
        Shape out;
        out.tf = in.tf;
        out.add_tag("type", "faces");

        for (size_t i = 0; i < geo.faces.size(); ++i) {
            const auto& face = geo.faces[i];
            if (face.loops.empty() || face.loops[0].size() < 3) continue;

            // Calculate plane
            auto v0 = geo.vertices[face.loops[0][0]];
            auto v1 = geo.vertices[face.loops[0][1]];
            auto v2 = geo.vertices[face.loops[0][2]];
            EK::Plane_3 plane(EK::Point_3(v0.x, v0.y, v0.z),
                              EK::Point_3(v1.x, v1.y, v1.z),
                              EK::Point_3(v2.x, v2.y, v2.z));
            
            if (plane.is_degenerate()) continue;

            // Centroid for anchor origin
            FT tx(0), ty(0), tz(0);
            for (int idx : face.loops[0]) {
                tx += geo.vertices[idx].x;
                ty += geo.vertices[idx].y;
                tz += geo.vertices[idx].z;
            }
            FT count = FT((int)face.loops[0].size());
            Point_3 centroid(tx/count, ty/count, tz/count);

            Matrix m = Matrix::fromNormal(centroid, plane.orthogonal_vector());

            // Zero-Base Tags for the individual face anchor
            Shape f_shape;
            f_shape.tf = m;
            f_shape.add_tag("type", "face");
            f_shape.add_tag("index", (int)i);

            if (proxy) {
                Geometry f_geo;
                Matrix inv_m = m.inverse();
                std::map<int, int> v_map;
                Geometry::Face local_face;
                for (const auto& loop : face.loops) {
                    std::vector<int> local_loop;
                    for (int v_idx : loop) {
                        if (v_map.find(v_idx) == v_map.end()) {
                            auto& v = geo.vertices[v_idx];
                            Point_3 lp = inv_m.transform(Point_3(v.x, v.y, v.z));
                            v_map[v_idx] = (int)f_geo.vertices.size();
                            f_geo.vertices.push_back({lp.x(), lp.y(), lp.z()});
                        }
                        local_loop.push_back(v_map[v_idx]);
                    }
                    local_face.loops.push_back(local_loop);
                }
                f_geo.faces.push_back(local_face);
                f_shape.geometry = vfs->materialize<Geometry>(f_geo);
            }
            out.components.push_back(f_shape);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "proxy"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/eachFace"},
            {"description", "Extracts faces from a shape as individual oriented child components."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "proxy"}, {"type", "jot:boolean"}, {"default", true}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void faces_init(fs::VFSNode* vfs) {
    Processor::register_op<FacesOp<>, Shape>(vfs, "jot/faces");
    Processor::register_op<EachFaceOp<>, Shape, bool>(vfs, "jot/eachFace");
}

} // namespace geo
} // namespace jotcad
