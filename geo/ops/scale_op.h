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
        
        // Rigid-Preserving Matrix Update:
        // Scale the translation part of the matrix, but keep the rotation part rigid.
        // This ensures the object stays at its 'scaled' position relative to parent.
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
        out.tf = in.tf;
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
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const typename P::json& factors) {
        std::vector<Matrix> tfs;
        if (factors.is_array() && !factors.empty()) {
            if (factors[0].is_number()) {
                // Single vec3: [x, y, z]
                FT x = FT(factors[0].template get<double>());
                FT y = (factors.size() > 1) ? FT(factors[1].template get<double>()) : x;
                FT z = (factors.size() > 2) ? FT(factors[2].template get<double>()) : y;
                tfs.push_back(Matrix::scale(x, y, z));
            } else {
                // List of vec3s: [[x,y,z], [x,y,z]]
                for (const auto& f : factors) {
                    if (f.is_array() && !f.empty()) {
                        FT x = FT(f[0].template get<double>());
                        FT y = (f.size() > 1) ? FT(f[1].template get<double>()) : x;
                        FT z = (f.size() > 2) ? FT(f[2].template get<double>()) : y;
                        tfs.push_back(Matrix::scale(x, y, z));
                    }
                }
            }
        } else if (factors.is_number()) {
            // Uniform scale
            FT s = FT(factors.template get<double>());
            tfs.push_back(Matrix::scale(s, s, s));
        }

        if (tfs.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        ScaleOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "factors"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/scale"},
            {"description", "Scales the shape. Supports uniform, non-uniform (vec3), and sequences."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "factors"}, {"type", "jot:any"}, {"description", "Number, Vec3, or Array of Vec3s"}}
            }},
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
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "factors"}, {"type", "jot:numbers"}}
            }},
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
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "factors"}, {"type", "jot:numbers"}}
            }},
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
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "factors"}, {"type", "jot:numbers"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void scale_init(fs::VFSNode* vfs) {
    Processor::register_op<ScaleOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scale");
    Processor::register_op<ScaleOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/s");
    Processor::register_op<ScaleXOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleX");
    Processor::register_op<ScaleXOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sx");
    Processor::register_op<ScaleYOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleY");
    Processor::register_op<ScaleYOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sy");
    Processor::register_op<ScaleZOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/scaleZ");
    Processor::register_op<ScaleZOp<>, Shape, typename JotVfsProtocol::json>(vfs, "jot/sz");
}

} // namespace geo
} // namespace jotcad
