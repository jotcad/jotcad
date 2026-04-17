#pragma once
#include "impl/protocols.h"
#include "impl/hexagon.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct HexagonOp : P {
    static constexpr const char* path = "jot/Hexagon/full";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& diameter, const std::string& variant, Shape& out) {
        std::vector<Shape> items;
        for (double d : diameter) {
            Geometry geo; makeHexagon(geo, d / 2.0, variant);
            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"d",d},{"v",variant}}, geo, {{"type","hexagon"},{"plane","Z0"}}))));
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
        auto variant = Processor::decode<std::string>(vfs, "variant", params, schema(), stack);
        
        Shape out;
        execute(vfs, diameter, variant, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", 10}}},
                {"variant", {{"type", "jot:string"}, {"default", "full"}}}
            }},
            {"inputs", {}},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void hexagon_init() {
    Processor::register_op<HexagonOp<>>();
}

} // namespace geo
} // namespace jotcad
