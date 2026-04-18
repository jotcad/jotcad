#pragma once
#include "impl/protocols.h"
#include "impl/box.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct BoxOp : P {
    static constexpr const char* path = "jot/Box";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& width, const std::vector<double>& height, const std::vector<double>& depth, Shape& out) {
        std::vector<Shape> items;
        for (double w : width) {
            for (double h : height) {
                for (double d : depth) {
                    Geometry geo; makeBox(geo, w, h, d);
                    items.push_back(P::make_shape(vfs, geo, {{"type", "box"}, {"plane", "Z0"}}));
                }
            }
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = std::nullopt;
            out.components = items;
            out.add_tag("type", "group");
        }
    }

    static std::vector<std::string> argument_keys() { return {"width", "height", "depth"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Box"},
            {"arguments", {
                {"width", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"height", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"depth", {{"type", "jot:numbers"}, {"default", {0.0}}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void box_init() {
    Processor::register_op<BoxOp<>, Shape, std::vector<double>, std::vector<double>, std::vector<double>>("jot/Box");
}

} // namespace geo
} // namespace jotcad
