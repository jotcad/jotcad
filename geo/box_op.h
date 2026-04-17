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
                if (depth.size() > 0 && depth[0] > 0) {
                    for (double d : depth) {
                        Geometry geo; makeBox(geo, w, h, d);
                        for (auto& v : geo.vertices) { v.x -= w/2.0; v.y -= h/2.0; v.z -= d/2.0; }
                        items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"w",w},{"h",h},{"d",d}}, geo, {{"type","box"}}))));
                    }
                } else {
                    Geometry geo; makeRectangle(geo, w, h);
                    for (auto& v : geo.vertices) { v.x -= w/2.0; v.y -= h/2.0; }
                    items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"w",w},{"h",h}}, geo, {{"type","box"},{"plane","Z0"}}))));
                }
            }
        }

        if (items.size() == 1) out = items[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<std::string> argument_keys() { return {"width", "height", "depth"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"width", {{"type", "jot:numbers"}, {"default", 10}}},
                {"height", {{"type", "jot:numbers"}, {"default", 10}}},
                {"depth", {{"type", "jot:numbers"}, {"default", 0}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void box_init() {
    Processor::register_op<BoxOp<>, std::vector<double>, std::vector<double>, std::vector<double>>();
}

} // namespace geo
} // namespace jotcad
