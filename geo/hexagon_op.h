#pragma once
#include "impl/protocols.h"
#include "impl/hexagon.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <const char* Variant, typename P = JotVfsProtocol>
struct HexagonOp : P {
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<double>& diameter, Shape& out) {
        std::vector<Shape> items;
        std::string variant = Variant;
        for (double d : diameter) {
            Geometry geo; makeHexagon(geo, d / 2.0, variant);
            items.push_back(Shape::from_json(P::json::parse(P::write_shape(vfs, {{"d",d},{"v",variant}}, geo, {{"type","hexagon"},{"plane","Z0"}}))));
        }

        if (items.size() == 1) out = items[0];
        else {
            out.geometry = {"jot/group", nlohmann::json::object()};
            nlohmann::json items_json = nlohmann::json::array();
            for (const auto& item : items) items_json.push_back(item.to_json());
            out.geometry.parameters["items"] = items_json;
        }
    }

    static std::vector<std::string> argument_keys() { return {"diameter"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", 10}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

// Template constants for variants
static constexpr char hex_full[] = "full";
static constexpr char hex_cap[] = "cap";
static constexpr char hex_middle[] = "middle";
static constexpr char hex_sector[] = "sector";
static constexpr char hex_half[] = "half";

static void hexagon_init() {
    auto s_full = HexagonOp<hex_full>::schema();
    s_full["metadata"]["alias"] = "jot/Hexagon";
    Processor::register_op<HexagonOp<hex_full>, std::vector<double>>("jot/Hexagon/full", s_full);
    
    Processor::register_op<HexagonOp<hex_cap>, std::vector<double>>("jot/Hexagon/cap");
    Processor::register_op<HexagonOp<hex_middle>, std::vector<double>>("jot/Hexagon/middle");
    Processor::register_op<HexagonOp<hex_sector>, std::vector<double>>("jot/Hexagon/sector");
    Processor::register_op<HexagonOp<hex_half>, std::vector<double>>("jot/Hexagon/half");
}

} // namespace geo
} // namespace jotcad
