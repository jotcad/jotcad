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
        for (double d : diameter) {
            Geometry geo; makeHexagon(geo, d/2.0, Variant);
            items.push_back(P::make_shape(vfs, geo, {{"type","hexagon"},{"plane","Z0"}}));
        }
        if (items.size() == 1) out = items[0];
        else {
            out.geometry = std::nullopt;
            out.components = items;
            out.add_tag("type", "group");
        }
    }

    static std::vector<std::string> argument_keys() { return {"diameter"}; }

    static typename P::json schema() {
        std::string variant = Variant;
        std::string canonical_path = "jot/Hexagon/" + variant;
        std::vector<std::string> aliases;
        
        if (variant == "full") {
            aliases = {"jot/Hexagon", "jot/hexagon"};
        }
        
        return {
            {"path", canonical_path},
            {"aliases", aliases},
            {"arguments", {
                {"diameter", {{"type", "jot:numbers"}, {"default", {10.0}}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static constexpr char hex_full[] = "full";
static constexpr char hex_cap[] = "cap";
static constexpr char hex_middle[] = "middle";
static constexpr char hex_sector[] = "sector";
static constexpr char hex_half[] = "half";

static void hexagon_init() {
    Processor::register_op<HexagonOp<hex_full>, Shape, std::vector<double>>("jot/Hexagon/full");
    Processor::register_op<HexagonOp<hex_cap>, Shape, std::vector<double>>("jot/Hexagon/cap");
    Processor::register_op<HexagonOp<hex_middle>, Shape, std::vector<double>>("jot/Hexagon/middle");
    Processor::register_op<HexagonOp<hex_sector>, Shape, std::vector<double>>("jot/Hexagon/sector");
    Processor::register_op<HexagonOp<hex_half>, Shape, std::vector<double>>("jot/Hexagon/half");
}

} // namespace geo
} // namespace jotcad
