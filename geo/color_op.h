#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ColorOp : P {
    static constexpr const char* path = "jot/color";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& color) {
        Shape out = in;
        out.tags["color"] = color;
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "color"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/color"},
            {"description", "Applies a color tag to the input shape."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape to color."}}},
                {"color", {{"type", "string"}, {"default", "red"}, {"description", "The color name or CSS hex value."}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The colored shape."}}}}}
        };
    }
};

static void color_init() {
    Processor::register_op<ColorOp<>, Shape, std::string>("jot/color");
}

} // namespace geo
} // namespace jotcad
