#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PlaneOpBase : P {
    static Shape make_plane(fs::VFSNode* vfs, const Matrix& orientation) {
        Geometry res;
        res.vertices.push_back({FT(0), FT(0), FT(0)});
        typename P::json tags = {{"type", "plane"}};
        Shape out = P::make_shape(vfs, res, tags);
        out.tf = orientation;
        return out;
    }

    static void execute_plane(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Matrix& orientation) {
        Shape out = make_plane(vfs, orientation);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void execute_planes(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Matrix>& orientations) {
        if (orientations.empty()) {
            execute_plane(vfs, fulfilling, Matrix::identity());
            return;
        }
        if (orientations.size() == 1) {
            execute_plane(vfs, fulfilling, orientations[0]);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& m : orientations) {
            out.components.push_back(make_plane(vfs, m));
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct XOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/X";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& offset) {
        std::vector<double> actual_offsets = offset;
        if (actual_offsets.empty()) actual_offsets.push_back(0.0);

        std::vector<Matrix> orientations;
        for (double d : actual_offsets) {
            Matrix m = Matrix::rotationY(0.25);
            if (d != 0.0) m = m * Matrix::translate(0, 0, FT(d));
            orientations.push_back(m);
        }
        PlaneOpBase<P>::execute_planes(vfs, fulfilling, orientations);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/X"}, 
            {"description", "Infinite plane on the YZ axis (normal +X)."}, 
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "offset"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct YOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Y";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& offset) {
        std::vector<double> actual_offsets = offset;
        if (actual_offsets.empty()) actual_offsets.push_back(0.0);

        std::vector<Matrix> orientations;
        for (double d : actual_offsets) {
            Matrix m = Matrix::rotationX(-0.25);
            if (d != 0.0) m = m * Matrix::translate(0, 0, FT(d));
            orientations.push_back(m);
        }
        PlaneOpBase<P>::execute_planes(vfs, fulfilling, orientations);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/Y"}, 
            {"description", "Infinite plane on the XZ axis (normal +Y)."}, 
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "offset"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct ZOp : PlaneOpBase<P> {
    static constexpr const char* path = "jot/Z";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<double>& offset) {
        std::vector<double> actual_offsets = offset;
        if (actual_offsets.empty()) actual_offsets.push_back(0.0);

        std::vector<Matrix> orientations;
        for (double d : actual_offsets) {
            Matrix m = Matrix::identity();
            if (d != 0.0) m = Matrix::translate(0, 0, FT(d));
            orientations.push_back(m);
        }
        PlaneOpBase<P>::execute_planes(vfs, fulfilling, orientations);
    }
    static std::vector<std::string> argument_keys() { return {"offset"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/Z"}, 
            {"description", "Infinite plane on the XY axis (normal +Z)."}, 
            {"inputs", nlohmann::json::object()},
            {"arguments", json::array({
                {{"name", "offset"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
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
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "offset"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
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
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "length"}, {"type", "jot:number"}, {"default", 0.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void plane_init(fs::VFSNode* vfs) {
    Processor::register_op<XOp<>, std::vector<double>>(vfs, "jot/X");
    Processor::register_op<YOp<>, std::vector<double>>(vfs, "jot/Y");
    Processor::register_op<ZOp<>, std::vector<double>>(vfs, "jot/Z");
    Processor::register_op<PlaneOp<>, Shape, double>(vfs, "jot/plane");
    Processor::register_op<NormalOp<>, Shape, double>(vfs, "jot/normal");
}

} // namespace geo
} // namespace jotcad
