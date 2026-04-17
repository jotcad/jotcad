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
                if (depth.size() > 0) {
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
            out.geometry = {"op/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto width = Processor::decode<std::vector<double>>(vfs, "width", params, schema(), stack);
        auto height = Processor::decode<std::vector<double>>(vfs, "height", params, schema(), stack);
        std::vector<double> depth;
        if (params.contains("depth")) depth = Processor::decode<std::vector<double>>(vfs, "depth", params, schema(), stack);
        
        Shape out;
        execute(vfs, width, height, depth, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"width", {{"type", "jot:numbers"}, {"default", 10}}},
                {"height", {{"type", "jot:numbers"}, {"default", 10}}},
                {"depth", {{"type", "jot:numbers"}, {"default", 0}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void box_init() {
    Processor::register_op<BoxOp<>>();
}

} // namespace geo
} // namespace jotcad
