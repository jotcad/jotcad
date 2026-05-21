#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PlaceOp : P {
    static constexpr const char* path = "jot/place";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& template_shape) {
        Shape out;
        out.add_tag("type", "group");
        
        apply_place_recursive(out, in, Matrix::identity(), template_shape);

        vfs->write(fulfilling.with_output("$out"), out);
    }

    static void apply_place_recursive(Shape& out, const Shape& target, const Matrix& parent_frame, const Shape& template_shape) {
        Matrix world_frame = parent_frame * target.tf;
        
        // If the target has geometry (a proxy or a real shape) or is an empty group (implicit frame), treat it as an anchor.
        if (target.geometry.has_value() || target.components.empty()) {
            Shape instance = template_shape;
            instance.tf = world_frame * instance.tf;
            out.components.push_back(instance);
        } else {
            // Recurse into collection components
            for (const auto& child : target.components) {
                apply_place_recursive(out, child, world_frame, template_shape);
            }
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "template_shape"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/place"},
            {"description", "Instantiates a template shape at every anchor point in the input collection."},
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "Collection of anchor shapes (e.g. from eachCorner())"}}}}},
            {"arguments", json::array({
                {{"name", "template_shape"}, {"type", "jot:shape"}, {"description", "The shape to instantiate at each anchor."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "A group containing the instantiated shapes."}}}}}
        };
    }
};

inline void place_init(fs::VFSNode* vfs) {
    Processor::register_op<PlaceOp<>, Shape, Shape>(vfs, "jot/place");
}

} // namespace geo
} // namespace jotcad
