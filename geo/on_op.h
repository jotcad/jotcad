#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

/**
 * AtOp: Simple Spatial Placement (jot/at)
 */
template <typename P = JotVfsProtocol>
struct AtOp : P {
    static constexpr const char* path = "jot/at";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::vector<Shape>& targets, Shape& out) {
        if (targets.empty()) {
            out = in;
            return;
        }

        std::vector<Shape> results;
        for (const auto& target : targets) {
            Matrix T = Matrix::from_vec(target.tf);
            Matrix acc_m = Matrix::from_vec(in.tf);
            Shape clone = in;
            clone.tf = (T * acc_m).to_vec();
            results.push_back(clone);
        }

        if (results.size() == 1) out = results[0];
        else {
            out.geometry = std::nullopt;
            out.components = results;
            out.add_tag("type", "group");
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "targets"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/at"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"targets", {{"type", "jot:shapes"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}, {"targets", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

/**
 * OnOp: Sequential Workbench Accumulator (jot/on)
 */
template <typename P = JotVfsProtocol>
struct OnOp : P {
    static constexpr const char* path = "jot/on";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::vector<Shape>& targets, const nlohmann::json& op, Shape& out) {
        if (targets.empty()) {
            out = in;
            return;
        }

        Shape accumulator = in;
        for (const auto& target : targets) {
            Matrix T = Matrix::from_vec(target.tf);
            Matrix T_inv = T.inverse();

            Shape workbench_subject = accumulator;
            workbench_subject.tf = (T_inv * Matrix::from_vec(accumulator.tf)).to_vec();

            nlohmann::json hydrated = Processor::hydrate(op, workbench_subject);
            Shape workbench_result = Shape::from_json(vfs->template read<nlohmann::json>({
                hydrated.at("path"), 
                hydrated.value("parameters", nlohmann::json::object()),
                {} // stack
            }));

            accumulator = workbench_result;
            accumulator.tf = (T * Matrix::from_vec(workbench_result.tf)).to_vec();
        }

        out = accumulator;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "targets", "op"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/on"},
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
    Processor::register_op<AtOp<>, Shape, Shape, std::vector<Shape>>("jot/at");
    Processor::register_op<OnOp<>, Shape, Shape, std::vector<Shape>, nlohmann::json>("jot/on");
}

} // namespace geo
} // namespace jotcad
