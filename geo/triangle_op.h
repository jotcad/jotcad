#pragma once
#include "impl/protocols.h"
#include "impl/triangle.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct TriangleOp : P {
    static constexpr const char* path = "jot/Triangle/equilateral";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& diameter, Shape& out) {
        std::vector<Shape> items;
        for (double d : diameter) {
            Geometry geo; makeTriangle(geo, d, d, d);
            double h = (d * 0.86602540378);
            for (auto& v : geo.vertices) { v.x -= d / 2.0; v.y -= h / 3.0; }
            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"d",d}}, geo, {{"type","triangle"},{"plane","Z0"}}))));
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
        auto diameter = Processor::decode<std::vector<double>>(vfs, "diameter", params, schema(), stack);
        Shape out;
        execute(vfs, diameter, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", 10}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void triangle_init() {
    Processor::register_op<TriangleOp<>>();
}

} // namespace geo
} // namespace jotcad
