#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "jot/group";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& shapes) {
        Shape out = in;
        for (const auto& s : shapes) {
            out.components.push_back(s);
        }
        out.add_tag("type", "group");
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"$in", "shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/group"},
            {"description", "Combines multiple shapes into a single hierarchical group."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"description", "The base shape for the group."}}},
                {"shapes", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "Additional shapes to group together."}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting group shape."}}}}}
        };
    }
};

static void group_init() {
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>>("jot/group");
}

} // namespace geo
} // namespace jotcad
