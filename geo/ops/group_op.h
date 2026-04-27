#pragma once
#include "protocols.h"
#include "processor.h"

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
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/group"},
            {"description", "Combines multiple shapes into a single hierarchical group."},
            {"arguments", {
                {{"name", "$in"}, {"type", "jot:shape"}, {"description", "The base shape for the group."}, {"affiliate", "$out"}},
                {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "Additional shapes to group together."}, {"affiliate", "$out"}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The resulting group shape."}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct GroupPrimitiveOp : P {
    static constexpr const char* path = "jot/Group";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
        Shape out;
        out.components = shapes;
        out.add_tag("type", "group");
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/Group"},
            {"description", "Creates a new group from a list of shapes."},
            {"arguments", {
                {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void group_init(fs::VFSNode* vfs) {
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>>(vfs, "jot/group");
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>>(vfs, "jot/and");
    Processor::register_op<GroupPrimitiveOp<>, std::vector<Shape>>(vfs, "jot/Group");
}

} // namespace geo
} // namespace jotcad
