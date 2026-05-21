#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ScaleOpBase : P {
    static void eager_scale(fs::VFSNode* vfs, Shape& s, const Matrix& s_mat) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            geo.apply_tf(s_mat);
            s.geometry = vfs->materialize(geo);
        }
        
        Transformation t = s.tf.t;
        s.tf = Matrix(Transformation(
            t.cartesian(0,0), t.cartesian(0,1), t.cartesian(0,2), t.cartesian(0,3) * s_mat.t.cartesian(0,0),
            t.cartesian(1,0), t.cartesian(1,1), t.cartesian(1,2), t.cartesian(1,3) * s_mat.t.cartesian(1,1),
            t.cartesian(2,0), t.cartesian(2,1), t.cartesian(2,2), t.cartesian(2,3) * s_mat.t.cartesian(2,2)
        ));

        for (auto& child : s.components) {
            eager_scale(vfs, child, s_mat);
        }
    }

    static void execute_multi(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Matrix>& transforms) {
        if (transforms.size() == 1) {
            Shape out = in;
            eager_scale(vfs, out, transforms[0]);
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& m : transforms) {
            Shape c = in;
            eager_scale(vfs, c, m);
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct ScaleOp : ScaleOpBase<P> {
    static constexpr const char* path = "jot/scale";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, 
                       std::optional<double> x, std::optional<double> y, std::optional<double> z) {
        
        double vx = x.value_or(1.0);
        // If only X is provided, it's a uniform scale. If any other is provided, it's non-uniform.
        double vy = y.value_or((y.has_value() || z.has_value()) ? 1.0 : vx);
        double vz = z.value_or((y.has_value() || z.has_value()) ? 1.0 : vx);

        ScaleOpBase<P>::execute_multi(vfs, fulfilling, in, {Matrix::scale(FT(vx), FT(vy), FT(vz))});
    }
    static std::vector<std::string> argument_keys() { return {"$in", "x", "y", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/scale"},
            {"description", "Scales the shape. Supports scale(s) and scale(x, y, z)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "x"}, {"type", "jot:number"}, {"optional", true}, {"description", "X factor (or uniform s)"}},
                {{"name", "y"}, {"type", "jot:number"}, {"optional", true}, {"description", "Y factor"}},
                {{"name", "z"}, {"type", "jot:number"}, {"optional", true}, {"description", "Z factor"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ScaleAxisOp : ScaleOpBase<P> {
    enum Axis { X, Y, Z };
    static void execute_axis(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& factors, Axis axis) {
        std::vector<Matrix> tfs;
        std::vector<double> vals;
        if (factors.is_array()) {
            for (const auto& f : factors) if (f.is_number()) vals.push_back(f.template get<double>());
        } else if (factors.is_number()) {
            vals.push_back(factors.template get<double>());
        }

        for (double v : vals) {
            FT sx = (axis == X) ? FT(v) : FT(1);
            FT sy = (axis == Y) ? FT(v) : FT(1);
            FT sz = (axis == Z) ? FT(v) : FT(1);
            tfs.push_back(Matrix::scale(sx, sy, sz));
        }

        if (tfs.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        ScaleOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
};

template <typename P = JotVfsProtocol>
struct ScaleXOp : ScaleAxisOp<P> {
    static constexpr const char* path = "jot/scaleX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& factors) {
        ScaleAxisOp<P>::execute_axis(vfs, fulfilling, in, factors, ScaleAxisOp<P>::X);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "factors"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/scaleX"},
            {"description", "Scales along the X axis. Supports sequences (e.g. sx(1, -1))."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "factors"}, {"type", "jot:numbers"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ScaleYOp : ScaleAxisOp<P> {
    static constexpr const char* path = "jot/scaleY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& factors) {
        ScaleAxisOp<P>::execute_axis(vfs, fulfilling, in, factors, ScaleAxisOp<P>::Y);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "factors"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/scaleY"},
            {"description", "Scales along the Y axis. Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "factors"}, {"type", "jot:numbers"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct ScaleZOp : ScaleAxisOp<P> {
    static constexpr const char* path = "jot/scaleZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& factors) {
        ScaleAxisOp<P>::execute_axis(vfs, fulfilling, in, factors, ScaleAxisOp<P>::Z);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "factors"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/scaleZ"},
            {"description", "Scales along the Z axis. Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "factors"}, {"type", "jot:numbers"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void scale_init(fs::VFSNode* vfs) {
    Processor::register_op<ScaleOp<>, Shape, std::optional<double>, std::optional<double>, std::optional<double>>(vfs, "jot/scale");
    Processor::register_op<ScaleOp<>, Shape, std::optional<double>, std::optional<double>, std::optional<double>>(vfs, "jot/s");
    Processor::register_op<ScaleXOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleX");
    Processor::register_op<ScaleXOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sx");
    Processor::register_op<ScaleYOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleY");
    Processor::register_op<ScaleYOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sy");
    Processor::register_op<ScaleZOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleZ");
    Processor::register_op<ScaleZOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sz");
}

} // namespace geo
} // namespace jotcad
