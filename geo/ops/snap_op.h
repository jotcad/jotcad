#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SnapOp : P {
    static constexpr const char* path = "jot/snap";

    static bool is_recipe_op(const std::string& path) {
        return path == "jot/top" || path == "jot/bottom" || 
               path == "jot/left" || path == "jot/right" || 
               path == "jot/front" || path == "jot/back" ||
               path == "jot/tx" || path == "jot/ty" || path == "jot/tz" ||
               path == "jot/faces" || path == "jot/highest" || 
               path == "jot/lowest" || path == "jot/facing";
    }

    static void bind_innermost_input(nlohmann::json& param, const nlohmann::json& target_val) {
        if (param.is_object() && param.contains("path")) {
            std::string path = param["path"].get<std::string>();
            if (!is_recipe_op(path)) {
                param = target_val;
                return;
            }
            if (!param.contains("parameters") || param["parameters"].is_null()) {
                param["parameters"] = nlohmann::json::object();
            }
            if (param["parameters"].contains("$in") && !param["parameters"]["$in"].is_null()) {
                bind_innermost_input(param["parameters"]["$in"], target_val);
            } else {
                param["parameters"]["$in"] = target_val;
            }
        } else {
            param = target_val;
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, 
                        const Shape& in, const Shape& source_anchor, 
                        const fs::Selector& target_anchor_recipe, const Shape& target_shape) {
        
        // 1. Evaluate the target anchor recipe on the target shape.
        nlohmann::json recipe_json = target_anchor_recipe.to_json();
        bind_innermost_input(recipe_json, vfs->materialize(target_shape).value);
        fs::Selector anchor_call = fs::Selector::from_json(recipe_json);
        Shape target_anchor = vfs->read<Shape>(anchor_call.with_output("$out"));

        // If target_anchor is a group with 1 component, unpack it
        if (target_anchor.components.size() == 1) {
            target_anchor = target_anchor.components[0];
        }

        // 2. Compute the snapping transformation.
        // We want source_anchor to land at target_anchor.
        Matrix source_inv = source_anchor.tf.inverse();
        Matrix snap_m = target_anchor.tf * source_inv;

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
                {{"name", "source_anchor"}, {"type", "jot:shape"}, {"description", "The anchor on the subject shape (e.g. top())."}},
                {{"name", "target_anchor"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "The anchor recipe on the target shape (e.g. bottom())."}},
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
