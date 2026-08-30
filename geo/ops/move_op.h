#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct MoveOpBase : P {
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
struct MoveOp : MoveOpBase<P> {
    static constexpr const char* path = "jot/move";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in,
                        const std::vector<EK::Vector_3>& vectors,
                        const std::optional<std::vector<FT>>& distance) {
        std::vector<Matrix> tfs;
        for (size_t i = 0; i < vectors.size(); ++i) {
            EK::Vector_3 v = vectors[i];
            EK::Vector_3 trans;
            if (distance.has_value() && !distance->empty()) {
                FT d = (*distance)[i < distance->size() ? i : distance->size() - 1];
                double len = std::sqrt(CGAL::to_double(v.squared_length()));
                if (len > 1e-9) {
                    FT scale = d / FT(len);
                    trans = v * scale;
                } else {
                    trans = EK::Vector_3(0, 0, 0);
                }
            } else {
                trans = v;
            }
            tfs.push_back(Matrix(Transformation(CGAL::TRANSLATION, trans)));
        }
        MoveOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "vector", "distance"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/move"},
            {"dsl_name", "move"},
            {"role", "method"},
            {"description", "Translates the input shape by one or more 3D vectors or directions with optional distance."},
            {"synonyms", {"translate", "shift", "offset", "position"}},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"binding", "implicit"}, {"description", "The shape to move."}}}}},
            {"arguments", json::array({
                {{"name", "vector"}, {"type", "jot:vec3s"}, {"default", {{0.0, 0.0, 0.0}}}, {"description", "The translation vector(s) [x, y, z] or direction string 'x y z'."}},
                {{"name", "distance"}, {"type", "jot:numbers"}, {"optional", true}, {"description", "Optional scalar distance along the direction of the vector."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The moved shape(s)."}}}}},
            {"examples", {
                "Box(10).move([5, 0, 0]) -> $out",
                "Box(10).move('1 1 1', 15) -> $out"
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveAxisOp : MoveOpBase<P> {
    enum Axis { X, Y, Z };
    static void execute_axis(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& offsets, Axis axis) {
        std::vector<Matrix> tfs;
        for (double d : offsets) {
            if (axis == X) tfs.push_back(Matrix::translate(d, 0, 0));
            else if (axis == Y) tfs.push_back(Matrix::translate(0, d, 0));
            else tfs.push_back(Matrix::translate(0, 0, d));
        }

        if (tfs.empty()) {
            vfs->write(fulfilling.with_output("$out"), in);
            return;
        }
        MoveOpBase<P>::execute_multi(vfs, fulfilling, in, tfs);
    }
};

template <typename P = JotVfsProtocol>
struct MoveXOp : MoveAxisOp<P> {
    static constexpr const char* path = "jot/moveX";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& x) {
        MoveAxisOp<P>::execute_axis(vfs, fulfilling, in, x, MoveAxisOp<P>::X);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "x"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveX"},
            {"description", "Translates the input shape along the X-axis. Supports sequences (e.g. mx(1, 10))."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "x"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveYOp : MoveAxisOp<P> {
    static constexpr const char* path = "jot/moveY";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& y) {
        MoveAxisOp<P>::execute_axis(vfs, fulfilling, in, y, MoveAxisOp<P>::Y);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "y"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveY"},
            {"description", "Translates the input shape along the Y-axis. Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "y"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct MoveZOp : MoveAxisOp<P> {
    static constexpr const char* path = "jot/moveZ";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& z) {
        MoveAxisOp<P>::execute_axis(vfs, fulfilling, in, z, MoveAxisOp<P>::Z);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "z"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/moveZ"},
            {"description", "Translates the input shape along the Z-axis. Supports sequences."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}}},
            {"arguments", json::array({
                {{"name", "z"}, {"type", "jot:numbers"}, {"default", {0.0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void move_init(fs::VFSNode* vfs) {
    Processor::register_op<MoveOp<>, Shape, std::vector<EK::Vector_3>, std::optional<std::vector<FT>>>(vfs, "jot/move");
    Processor::register_op<MoveOp<>, Shape, std::vector<EK::Vector_3>, std::optional<std::vector<FT>>>(vfs, "jot/m"); 
    Processor::register_op<MoveXOp<>, Shape, std::vector<double>>(vfs, "jot/moveX");
    Processor::register_op<MoveXOp<>, Shape, std::vector<double>>(vfs, "jot/mx");
    Processor::register_op<MoveYOp<>, Shape, std::vector<double>>(vfs, "jot/moveY");
    Processor::register_op<MoveYOp<>, Shape, std::vector<double>>(vfs, "jot/my");
    Processor::register_op<MoveZOp<>, Shape, std::vector<double>>(vfs, "jot/moveZ");
    Processor::register_op<MoveZOp<>, Shape, std::vector<double>>(vfs, "jot/mz");
}

} // namespace geo
} // namespace jotcad
