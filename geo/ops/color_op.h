#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ColorOp : P {
    static constexpr const char* path = "jot/color";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& color) {
        Shape out = in;
        out.tags["color"] = color;
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "color"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/color"},
            {"description", "Applies a color tag to the input shape."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"description", "The shape to color."}}}
            }},
            {"arguments", nlohmann::json::array({
                {{"name", "color"}, {"type", "jot:string"}, {"default", "red"}, {"description", "The color name or CSS hex value."}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The colored shape."}}}}}
        };
    }
};

static void color_init(fs::VFSNode* vfs) {
    Processor::register_op<ColorOp<>, Shape, std::string>(vfs, "jot/color");
}

} // namespace geo
} // namespace jotcad
