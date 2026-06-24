#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct RotateOpBase : P {
    static void execute_multi(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Matrix>& transforms) {
        if (transforms.size() == 1) {
            Shape out = in;
            out.apply_transform(transforms[0]);
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& m : transforms) {
            Shape c = in;
            c.apply_transform(m);
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct RotateAxisOp : RotateOpBase<P> {
    enum Axis { X, Y, Z };
    static void execute_axis(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns, Axis axis) {
        std::vector<Matrix> tfs;
        for (double turn : turns) {
            if (axis == X) tfs.push_back(Matrix::rotationX(turn));
            else if (axis == Y) tfs.push_back(Matrix::rotationY(turn));
            else tfs.push_back(Matrix::rotationZ(turn));
        }

        if (tfs.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        RotateOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
};

template <typename P = JotVfsProtocol>
struct RotateXOp : RotateAxisOp<P> {
    static constexpr const char* path = "jot/rotateX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        RotateAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, RotateAxisOp<P>::X);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rotateX"},
            {"description", "Rotates the input shape around the X-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct RotateYOp : RotateAxisOp<P> {
    static constexpr const char* path = "jot/rotateY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        RotateAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, RotateAxisOp<P>::Y);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rotateY"},
            {"description", "Rotates the input shape around the Y-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct RotateZOp : RotateAxisOp<P> {
    static constexpr const char* path = "jot/rotateZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        RotateAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, RotateAxisOp<P>::Z);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/rotateZ"},
            {"description", "Rotates the input shape around the Z-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TurnOpBase : P {
    static void execute_multi(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Matrix>& transforms) {
        if (transforms.size() == 1) {
            Shape out = in;
            Matrix world_m = out.tf * transforms[0] * out.tf.inverse();
            out.apply_transform(world_m);
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");
        for (const auto& m : transforms) {
            Shape c = in;
            Matrix world_m = c.tf * m * c.tf.inverse();
            c.apply_transform(world_m);
            out.components.push_back(c);
        }
        vfs->write(fulfilling.with_output("$out"), out);
    }
};

template <typename P = JotVfsProtocol>
struct TurnAxisOp : TurnOpBase<P> {
    enum Axis { X, Y, Z };
    static void execute_axis(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns, Axis axis) {
        std::vector<Matrix> tfs;
        for (double turn : turns) {
            if (axis == X) tfs.push_back(Matrix::rotationX(turn));
            else if (axis == Y) tfs.push_back(Matrix::rotationY(turn));
            else tfs.push_back(Matrix::rotationZ(turn));
        }

        if (tfs.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        TurnOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
};

template <typename P = JotVfsProtocol>
struct TurnXOp : TurnAxisOp<P> {
    static constexpr const char* path = "jot/tx";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        TurnAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, TurnAxisOp<P>::X);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/tx"},
            {"description", "Rotates the input shape around its own local X-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TurnYOp : TurnAxisOp<P> {
    static constexpr const char* path = "jot/ty";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        TurnAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, TurnAxisOp<P>::Y);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/ty"},
            {"description", "Rotates the input shape around its own local Y-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct TurnZOp : TurnAxisOp<P> {
    static constexpr const char* path = "jot/tz";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& turns) {
        TurnAxisOp<P>::execute_axis(vfs, fulfilling, in, turns, TurnAxisOp<P>::Z);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "turns"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/tz"},
            {"description", "Rotates the input shape around its own local Z-axis (Tau-based turns)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "turns"}, {"type", "jot:numbers"}, {"default", json::array({0.0})}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void rotate_init(fs::VFSNode* vfs) {
    Processor::register_op<RotateXOp<>, Shape, std::vector<double>>(vfs, "jot/rotateX");
    Processor::register_op<RotateXOp<>, Shape, std::vector<double>>(vfs, "jot/rx");
    Processor::register_op<RotateYOp<>, Shape, std::vector<double>>(vfs, "jot/rotateY");
    Processor::register_op<RotateYOp<>, Shape, std::vector<double>>(vfs, "jot/ry");
    Processor::register_op<RotateZOp<>, Shape, std::vector<double>>(vfs, "jot/rotateZ");
    Processor::register_op<RotateZOp<>, Shape, std::vector<double>>(vfs, "jot/rz");
    Processor::register_op<RotateZOp<>, Shape, std::vector<double>>(vfs, "jot/rotate");
    Processor::register_op<RotateZOp<>, Shape, std::vector<double>>(vfs, "jot/r");

    Processor::register_op<TurnXOp<>, Shape, std::vector<double>>(vfs, "jot/tx");
    Processor::register_op<TurnXOp<>, Shape, std::vector<double>>(vfs, "jot/turnX");
    Processor::register_op<TurnYOp<>, Shape, std::vector<double>>(vfs, "jot/ty");
    Processor::register_op<TurnYOp<>, Shape, std::vector<double>>(vfs, "jot/turnY");
    Processor::register_op<TurnZOp<>, Shape, std::vector<double>>(vfs, "jot/tz");
    Processor::register_op<TurnZOp<>, Shape, std::vector<double>>(vfs, "jot/turnZ");
}

} // namespace geo
} // namespace jotcad
