#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "jot/group";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& in, const std::vector<Shape>& others, Shape& out) {
        out.geometry.path = "";
        for (const auto& s : in) out.components.push_back(s);
        for (const auto& s : others) out.components.push_back(s);
        out.add_tag("type", "group");
    }

    static std::vector<std::string> argument_keys() { return {"$in", "others"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}},
                {"others", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}}}, {"others", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }

    static typename P::json constructor_schema() {
        auto s = schema();
        s["metadata"]["alias"] = "jot/Group";
        return s;
    }
};

static void group_init() {
    // Both variants use the same logic, schemas handle the mapping of 'shapes' and '$in'
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>, std::vector<Shape>>("jot/Group", GroupOp<>::constructor_schema());
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>, std::vector<Shape>>("jot/group", GroupOp<>::schema());
}

} // namespace geo
} // namespace jotcad
