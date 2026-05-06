#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PlaneOpBase : P {
    static void execute_plane(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Matrix& orientation) {
        Geometry res;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        
        typename P::json tags = {{"type", "plane"}};
        Shape out = P::make_shape(vfs, res, tags);
        out.tf = orientation;
        
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct XOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/X";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double offset = 0.0) {
        // Rotate local Z (0,0,1) to world X (1,0,0) -> Rotation around Y by 0.25 turns
        Matrix m = Matrix::rotationY(0.25);
        if (offset != 0.0) m = m * Matrix::translate(0, 0, FT(offset));
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, m);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/X"}, 
            {"description", "Infinite plane on the YZ axis (normal +X)."}, 
            {"arguments", {{{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct YOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Y";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double offset = 0.0) {
        // Rotate local Z (0,0,1) to world Y (0,1,0) -> Rotation around X by -0.25 turns
        Matrix m = Matrix::rotationX(-0.25);
        if (offset != 0.0) m = m * Matrix::translate(0, 0, FT(offset));
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, m);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/Y"}, 
            {"description", "Infinite plane on the XZ axis (normal +Y)."}, 
            {"arguments", {{{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct ZOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Z";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, double offset = 0.0) {
        Matrix m = Matrix::identity();
        if (offset != 0.0) m = Matrix::translate(0, 0, FT(offset));
        PlaneOpBase<P>::execute_plane(vfs, fulfilling, m);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/Z"}, 
            {"description", "Infinite plane on the XY axis (normal +Z)."}, 
            {"arguments", {{{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct PlaneOp : P {
    static constexpr const char* path = "jot/plane";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double offset = 0.0) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        auto p_opt = geo.find_plane();
        if (!p_opt) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // Get origin from first vertex of first face
        EK::Point_3 origin(0, 0, 0);
        if (!geo.faces.empty() && !geo.faces[0].loops.empty() && !geo.faces[0].loops[0].empty()) {
            auto& v = geo.vertices[geo.faces[0].loops[0][0]];
            origin = EK::Point_3(v.x, v.y, v.z);
        }

        Matrix m = Matrix::fromNormal(origin, p_opt->orthogonal_vector());
        if (offset != 0.0) m = m * Matrix::translate(0, 0, FT(offset));
        
        Shape out;
        out.tf = in.tf * m;
        out.add_tag("type", "plane");
        
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/plane"}, 
            {"description", "Extracts the coordinate system of the first face as a plane shape."}, 
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct NormalOp : P {
    static constexpr const char* path = "jot/normal";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double length = 0.0) {
        if (!in.geometry.has_value()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        auto p_opt = geo.find_plane();
        if (!p_opt) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }

        // Normal is a unit vector in the plane's coordinate system.
        // We return a shape with type "normal" and the orientation.
        Matrix m = Matrix::fromNormal(EK::Point_3(0,0,0), p_opt->orthogonal_vector());
        if (length != 0.0) m = m * Matrix::translate(0, 0, FT(length));
        
        Shape out;
        out.tf = in.tf * m;
        out.add_tag("type", "normal");
        
        // Optional: add a tiny visual segment for debugging
        Geometry v_geo;
        v_geo.vertices.push_back({FT(0), FT(0), FT(0)});
        v_geo.vertices.push_back({FT(0), FT(0), FT(1)});
        v_geo.segments.push_back({0, 1});
        out.geometry = vfs->materialize<Geometry>(v_geo);
        
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "length"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/normal"}, 
            {"description", "Extracts the normal vector of the first face as an oriented normal shape."}, 
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "length"}, {"type", "jot:number"}, {"default", 0.0}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void plane_init(fs::VFSNode* vfs) {
    Processor::register_op<XOp<>, double>(vfs, "jot/X");
    Processor::register_op<YOp<>, double>(vfs, "jot/Y");
    Processor::register_op<ZOp<>, double>(vfs, "jot/Z");
    Processor::register_op<PlaneOp<>, Shape, double>(vfs, "jot/plane");
    Processor::register_op<NormalOp<>, Shape, double>(vfs, "jot/normal");
}

} // namespace geo
} // namespace jotcad
