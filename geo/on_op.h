#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OnOp : P {
    static constexpr const char* path = "jot/on";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::vector<Shape>& targets, const nlohmann::json& op, Shape& out) {
        if (targets.empty()) {
            out = in;
            return;
        }

        // ACCUMULATOR PATTERN: Sequentially apply the op to the result of the previous step
        // We use strict conjugation: T * op * T_inv
        Shape accumulator = in;

        for (const auto& target : targets) {
            Matrix T = Matrix::from_vec(target.tf);
            Matrix T_inv = T.inverse();

            // 1. Mount: Map subject into local workbench space
            Shape workbench_subject = accumulator;
            workbench_subject.tf = (T_inv * Matrix::from_vec(accumulator.tf)).to_vec();

            // 2. Operate: Perform workbench logic at origin
            nlohmann::json hydrated = Processor::hydrate(op, workbench_subject);
            Shape workbench_result = Shape::from_json(vfs->template read<nlohmann::json>({
                hydrated.at("path"), 
                hydrated.value("parameters", nlohmann::json::object()),
                {} // stack
            }));

            // 3. Unmount: Map result back to global space
            accumulator = workbench_result;
            accumulator.tf = (T * Matrix::from_vec(workbench_result.tf)).to_vec();
        }

        out = accumulator;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "targets", "op"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"targets", {{"type", "jot:shapes"}}},
                {"op", {{"type", "jot:operation"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}, {"targets", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void on_init() {
    Processor::register_op<OnOp<>, Shape, Shape, std::vector<Shape>, nlohmann::json>();
}

} // namespace geo
} // namespace jotcad
