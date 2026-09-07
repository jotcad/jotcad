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
        Shape subject = in;
        for (const auto& anchor : target.shapes()) {
            if (!anchor.geometry.has_value()) continue;
            Matrix world_frame = anchor.tf;
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
        }
        vfs->write(fulfilling.with_output("$out"), subject);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "target", "op"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/at"},
            {"dsl_name", "at"},
            {"role", "method"},
            {"description", "Applies an operation to the subject relative to one or more anchor locations (The Anchor Pattern)."},
            {"synonyms", {"align", "snap", "mount", "place at", "position"}},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"binding", "implicit"}, {"description", "The primary subject shape to transform."}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "target"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "Anchor provider (e.g., eachCorner(), faces())"}},
                {{"name", "op"}, {"type", "jot:op<$in:shape, $out:shape>"}, {"description", "Operation to apply at each anchor. Implicitly receives the local scoped copy of the subject as its $in."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The resulting combined shape."}}}}},
            {"examples", {
                "Box(10).at(eachCorner(), cut(Orb(2))) -> $out",
                "Cylinder(5).at(faces().highest(z()), color(\"red\")) -> $out"
            }}
        };
    }
};

inline void at_init(fs::VFSNode* vfs) {
    Processor::register_op<AtOp<>, Shape, Shape, fs::Selector>(vfs, "jot/at");
}

} // namespace geo
} // namespace jotcad
