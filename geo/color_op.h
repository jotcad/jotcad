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
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"color", {{"type", "jot:string"}, {"default", "black"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}},
            {"metadata", {{"alias", "jot/color"}}}
        };
    }
};

static void color_init() {
    Processor::register_op<ColorOp<>, Shape, std::string>();
}

} // namespace geo
} // namespace jotcad
