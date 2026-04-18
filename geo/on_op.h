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

        std::vector<Shape> results;
        for (const auto& target : targets) {
            Matrix target_tf = Matrix::from_vec(target.tf);
            Matrix inv = target_tf.inverse();

            // 1. Clamping: Move subject to origin relative to target
            Shape workbench_subject = in;
            workbench_shape_transform(workbench_subject, inv);

            // 2. Hydrate and Execute
            nlohmann::json hydrated = Processor::hydrate(op, workbench_subject);
            Shape workbench_result = Shape::from_json(vfs->template read<nlohmann::json>({hydrated.at("path"), hydrated.value("parameters", nlohmann::json::object())}));

            // 3. Unclamping: Move result back to world
            workbench_shape_transform(workbench_result, target_tf);
            results.push_back(workbench_result);
        }

        if (results.size() == 1) {
            out = results[0];
        } else {
            out.geometry.path = "op/group";
            out.components = results;
            out.add_tag("type", "group");
        }
    }

    // Recursively apply transform to a shape and its components (metadata only)
    static void workbench_shape_transform(Shape& s, const Matrix& m) {
        Matrix current = Matrix::from_vec(s.tf);
        s.tf = (m * current).to_vec();
        // Note: For 'on' we only transform the top-level tf. 
        // If the shape has components, they stay relative to the parent.
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
