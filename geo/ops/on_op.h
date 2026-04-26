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
        Shape result = in;
        // Targeted Replacement: Recursively search 'in' for components matching 'target' and apply 'op' to each.
        find_and_replace(vfs, result, target, op);
        vfs->write(fulfilling.with_output("$out"), result);
    }

    static bool find_and_replace(fs::VFSNode* vfs, Shape& current, const Shape& target, const fs::Selector& op) {
        // Basic match by geometry CID. Future: Expand to full Selector matching.
        bool match = (target.geometry.has_value() && current.geometry == target.geometry);
        
        if (match) {
            // Conjugation Pattern: 
            // 1. Store the original attachment transform.
            Matrix attachment = Matrix::from_vec(current.tf);
            
            // 2. Normalize the component to its local origin.
            Shape local_comp = current;
            local_comp.tf = Matrix::identity().to_vec();
            
            // 3. Apply the operation to the normalized component.
            fs::Selector call = op;
            call.parameters["$in"] = vfs->materialize(local_comp).value;
            Shape transformed = vfs->read<Shape>(call);
            
            // 4. Re-apply the original attachment to the transformed result.
            // (attachment * transformed.tf) preserves any transformation 
            // introduced by 'op' while maintaining the original placement.
            transformed.tf = (attachment * Matrix::from_vec(transformed.tf)).to_vec();
            
            current = transformed;
            return true; 
        }

        bool any_child_replaced = false;
        for (auto& child : current.components) {
            if (find_and_replace(vfs, child, target, op)) any_child_replaced = true;
        }
        return any_child_replaced;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/on"},
            {"description", "Performs targeted in-place replacement of components within the subject hierarchy."},
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
