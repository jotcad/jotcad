#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ColorOp : P {
    static constexpr const char* path = "jot/color";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::string& color, Shape& out) {
        out = in;
        out.tags["color"] = color;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "color"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/color"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"color", {{"type", "jot:string"}, {"default", "black"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"alias", "$in"}}}}}
        };
    }
};

static void color_init() {
    Processor::register_op<ColorOp<>, Shape, Shape, std::string>("jot/color");
}

} // namespace geo
} // namespace jotcad
