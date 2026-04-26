#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct AtOp : P {
    static constexpr const char* path = "jot/at";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& target, const fs::Selector& op) {
        std::vector<Shape> results;
        place_at(vfs, in, target, Matrix::identity(), op, results);
        Shape out;
        out.components = results;
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void place_at(fs::VFSNode* vfs, const Shape& in, const Shape& target, const Matrix& parent_frame, const fs::Selector& op, std::vector<Shape>& results) {
        Matrix world_frame = parent_frame * Matrix::from_vec(target.tf);
        if (target.geometry.has_value()) {
            Shape clone = in;
            clone.tf = (world_frame * Matrix::from_vec(in.tf)).to_vec();
            if (!op.path.empty()) {
                fs::Selector call = op;
                call.parameters["$in"] = vfs->materialize(clone).value;
                clone = vfs->read<Shape>(call);
            }
            results.push_back(clone);
        } else {
            for (const auto& child : target.components) {
                place_at(vfs, in, child, world_frame, op, results);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/at"},
            {"description", "Places copies of the input shape at every location defined by the target shape."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"target", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"op", {{"type", "jot:selector"}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct OnOp : P {
    static constexpr const char* path = "jot/on";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& target, const fs::Selector& op) {
        std::vector<Shape> results;
        apply_on(vfs, in, target, Matrix::identity(), op, results);
        Shape out;
        out.components = results;
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void apply_on(fs::VFSNode* vfs, const Shape& in, const Shape& target, const Matrix& parent_frame, const fs::Selector& op, std::vector<Shape>& results) {
        Matrix world_frame = parent_frame * Matrix::from_vec(target.tf);
        if (target.geometry.has_value()) {
            Matrix world_frame_inv = world_frame.inverse();
            Shape local_subject = in;
            local_subject.tf = (world_frame_inv * Matrix::from_vec(in.tf)).to_vec();
            
            fs::Selector call = op;
            call.parameters["$in"] = vfs->materialize(local_subject).value;
            Shape local_result = vfs->read<Shape>(call);
            
            local_result.tf = (world_frame * Matrix::from_vec(local_result.tf)).to_vec();
            results.push_back(local_result);
        } else {
            for (const auto& child : target.components) {
                apply_on(vfs, in, child, world_frame, op, results);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/on"},
            {"description", "Sequentially applies an operation to the input shape at every location defined by the target shape (Reduce)."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"target", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"op", {{"type", "jot:selector"}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void on_init(fs::VFSNode* vfs) {
    Processor::register_op<AtOp<>, Shape, Shape, fs::Selector>(vfs, "jot/at");
    Processor::register_op<OnOp<>, Shape, Shape, fs::Selector>(vfs, "jot/on");
}

} // namespace geo
} // namespace jotcad
