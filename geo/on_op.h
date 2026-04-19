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
        vfs->write<Shape>(fulfilling, out);
    }

    static void place_at(fs::VFSNode* vfs, const Shape& in, const Shape& target, const Matrix& parent_frame, const fs::Selector& op, std::vector<Shape>& results) {
        Matrix world_frame = parent_frame * Matrix::from_vec(target.tf);
        if (target.geometry.has_value()) {
            Shape clone = in;
            clone.tf = (world_frame * Matrix::from_vec(in.tf)).to_vec();
            if (!op.path.empty()) {
                nlohmann::json hydrated = Processor::hydrate(op.to_json(), clone);
                clone = vfs->read<Shape>(fs::Selector::from_json(hydrated));
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
                {"$in", {{"type", "jot:shape"}}},
                {"target", {{"type", "jot:shape"}}},
                {"op", {{"type", "jot:selector"}, {"default", nlohmann::json::object()}}}
            }}
        };
    }
};

template <typename P = JotVfsProtocol>
struct OnOp : P {
    static constexpr const char* path = "jot/on";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& target, const fs::Selector& op) {
        Shape res = in;
        apply_on(vfs, res, target, Matrix::identity(), op);
        vfs->write<Shape>(fulfilling, res);
    }

    static void apply_on(fs::VFSNode* vfs, Shape& subject, const Shape& target, const Matrix& parent_frame, const fs::Selector& op) {
        Matrix world_frame = parent_frame * Matrix::from_vec(target.tf);
        if (target.geometry.has_value()) {
            Matrix world_frame_inv = world_frame.inverse();
            Shape local_subject = subject;
            local_subject.tf = (world_frame_inv * Matrix::from_vec(subject.tf)).to_vec();
            
            nlohmann::json hydrated = Processor::hydrate(op.to_json(), local_subject);
            Shape local_result = vfs->read<Shape>(fs::Selector::from_json(hydrated));
            
            subject = local_result;
            subject.tf = (world_frame * Matrix::from_vec(local_result.tf)).to_vec();
        } else {
            for (const auto& child : target.components) {
                apply_on(vfs, subject, child, world_frame, op);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/on"},
            {"description", "Sequentially applies an operation to the input shape at every location defined by the target shape (Reduce)."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"target", {{"type", "jot:shape"}}},
                {"op", {{"type", "jot:selector"}}}
            }}
        };
    }
};

static void on_init() {
    Processor::register_op<AtOp<>, Shape, Shape, fs::Selector>("jot/at");
    Processor::register_op<OnOp<>, Shape, Shape, fs::Selector>("jot/on");
}

} // namespace geo
} // namespace jotcad
