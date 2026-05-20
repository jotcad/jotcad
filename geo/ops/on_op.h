#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

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
            Matrix attachment = current.tf;
            
            // 2. Normalize the component tree to its local origin.
            Shape local_comp = current;
            local_comp.apply_transform(attachment.inverse());
            
            // 3. Apply the operation to the normalized component.
            fs::Selector call = op;
            call.parameters["$in"] = vfs->materialize(local_comp).value;
            Shape transformed = vfs->read<Shape>(call);
            
            // 4. Re-apply the original attachment to the transformed result.
            transformed.apply_transform(attachment);
            
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
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", {
                {{"name", "target"}, {"type", "jot:shape"}, {"description", "The component shape to replace"}},
                {{"name", "op"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "Operation to apply to matching components"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void on_init(fs::VFSNode* vfs) {
    Processor::register_op<OnOp<>, Shape, Shape, fs::Selector>(vfs, "jot/on");
}

} // namespace geo
} // namespace jotcad
