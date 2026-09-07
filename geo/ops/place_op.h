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
        
        for (const auto& target : in.shapes()) {
            if (target.geometry.has_value() || target.components.empty()) {
                Shape instance = template_shape;
                instance.apply_transform(target.tf);
                out.components.push_back(instance);
            }
        }

        vfs->write(fulfilling.with_output("$out"), out);
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
