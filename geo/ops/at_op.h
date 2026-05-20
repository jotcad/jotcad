#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct AtOp : P {
    static constexpr const char* path = "jot/at";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& target, const fs::Selector& op) {
        Shape current = in;
        apply_at_recursive(vfs, current, target, op);
        vfs->write(fulfilling.with_output("$out"), current);
    }

    static void apply_at_recursive(fs::VFSNode* vfs, Shape& subject, const Shape& target, const fs::Selector& op) {
        if (target.geometry.has_value()) {
            Matrix world_frame = target.tf;
            // Anchor Pattern:
            // 1. Invert the anchor's matrix to reach its local origin.
            Matrix world_inv = world_frame.inverse();
            
            // 2. Move the subject into this local origin.
            Shape local_subject = subject;
            local_subject.apply_transform(world_inv);
            
            // 3. Apply the operation at the local origin.
            fs::Selector call = op;
            call.parameters["$in"] = vfs->materialize(local_subject).value;
            Shape local_result = vfs->read<Shape>(call.with_output("$out"));
            
            // 4. Project the result back to the original world frame.
            local_result.apply_transform(world_frame);
            
            // 5. Update the subject for the next anchor (Sequential Reduction).
            subject = local_result;
        } else {
            for (const auto& child : target.components) {
                apply_at_recursive(vfs, subject, child, op);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/at"},
            {"description", "Applies an operation to the subject relative to one or more anchor locations (The Anchor Pattern)."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}}
            }},
            {"arguments", {
                {{"name", "target"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "Anchor provider (e.g., eachCorner())"}},
                {{"name", "op"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "Operation to apply at each anchor"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void at_init(fs::VFSNode* vfs) {
    Processor::register_op<AtOp<>, Shape, Shape, fs::Selector>(vfs, "jot/at");
}

} // namespace geo
} // namespace jotcad
