#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "recipe.h"

namespace jotcad {
namespace geo {

/**
 * Recursively resolves the reference coordinate frame of an anchor shape.
 * Walks down the shape hierarchy to find the first concrete reference frame.
 */
inline Matrix resolve_anchor_frame(const Shape& shape) {
    if (!shape.tf.is_identity()) {
        return shape.tf;
    }
    for (const auto& child : shape.components) {
        Matrix child_frame = resolve_anchor_frame(child);
        if (!child_frame.is_identity()) {
            return child_frame;
        }
    }
    return shape.tf;
}

template <typename P = JotVfsProtocol>
struct SnapOp : P {
    static constexpr const char* path = "jot/snap";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                        const Shape& in, const Shape& source_anchor, 
                        const fs::Selector& target_anchor_recipe, const Shape& target_shape) {
        
        // 1. Evaluate target anchor recipe on target shape
        fs::Selector tgt_call = bind_recipe(vfs, target_anchor_recipe, vfs->materialize(target_shape).value);
        Shape target_anchor = vfs->read<Shape>(tgt_call.with_output("$out"));

        // 2. Symmetrically resolve reference frames for both anchors
        Matrix source_frame = resolve_anchor_frame(source_anchor);
        Matrix target_frame = resolve_anchor_frame(target_anchor);

        // 3. Compute relative mating transform
        Matrix source_inv = source_frame.inverse();
        Matrix snap_m = target_frame * source_inv;

        // 3. Construct the output group containing:
        //    1) The snapped subject shape
        //    2) The target shape (which acts as the assembly base or gap tool)
        Shape out;
        out.tf = Matrix::identity();
        out.add_tag("type", "group");

        Shape subject_snapped = in;
        subject_snapped.apply_transform(snap_m);
        out.components.push_back(subject_snapped);

        out.components.push_back(target_shape);

        // 4. Return the snapped group
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { 
        return {"$in", "source_anchor", "target_anchor", "target_shape"}; 
    }

    static typename P::json schema() {
        return {
            {"path", "jot/snap"},
            {"description", "Snaps the subject to a target shape by aligning a source anchor on the subject with a target anchor on the target shape. Returns a group containing the snapped subject and the target shape."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape to snap/move."}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "source_anchor"}, {"type", "jot:shape"}, {"description", "The anchor on the subject shape (e.g. id('c'))."}},
                {{"name", "target_anchor"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "The anchor recipe on the target shape (e.g. id('b').ty(0.5))."}},
                {{"name", "target_shape"}, {"type", "jot:shape"}, {"description", "The target shape to snap onto."}}
            })},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "A group containing the snapped subject and the target shape."}}}
            }}
        };
    }
};

inline void snap_init(fs::VFSNode* vfs) {
    Processor::register_op<SnapOp<>, Shape, Shape, fs::Selector, Shape>(vfs, "jot/snap");
}

} // namespace geo
} // namespace jotcad
