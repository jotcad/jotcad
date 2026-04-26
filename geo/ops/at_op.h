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
        apply_at_recursive(vfs, current, target, Matrix::identity(), op);
        vfs->write(fulfilling.with_output("$out"), current);
    }

    static void apply_at_recursive(fs::VFSNode* vfs, Shape& subject, const Shape& target, const Matrix& parent_frame, const fs::Selector& op) {
        Matrix world_frame = parent_frame * Matrix::from_vec(target.tf);
        if (target.geometry.has_value()) {
            // Anchor Pattern:
            // 1. Invert the anchor's matrix to reach its local origin.
            Matrix world_inv = world_frame.inverse();
            
            // 2. Move the subject into this local origin.
            Shape local_subject = subject;
            local_subject.tf = (world_inv * Matrix::from_vec(subject.tf)).to_vec();
            
            // 3. Apply the operation at the local origin.
            fs::Selector call = op;
            call.parameters["$in"] = vfs->materialize(local_subject).value;
            Shape local_result = vfs->read<Shape>(call);
            
            // 4. Project the result back to the original world frame.
            local_result.tf = (world_frame * Matrix::from_vec(local_result.tf)).to_vec();
            
            // 5. Update the subject for the next anchor (Sequential Reduction).
            subject = local_result;
        } else {
            for (const auto& child : target.components) {
                apply_at_recursive(vfs, subject, child, world_frame, op);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/at"},
            {"description", "Applies an operation to the subject relative to one or more anchor locations (The Anchor Pattern)."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "target"}, {"type", "jot:shape"}, {"affiliate", "$out"}},
                {{"name", "op"}, {"type", "jot:selector"}}
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
