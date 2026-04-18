#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

/**
 * GroupOp: Constructor (jot/Group)
 * Takes a list of shapes and returns a new Group shape containing them.
 */
template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "jot/Group";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& shapes, Shape& out) {
        out.geometry = std::nullopt;
        out.components = shapes;
        out.tf = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        out.add_tag("type", "group");
    }

    static std::vector<std::string> argument_keys() { return {"shapes"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/Group"},
            {"arguments", {
                {"shapes", {{"type", "jot:shapes"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

/**
 * groupOp: Operator (jot/group)
 * Takes a subject and appends other shapes to it.
 */
template <typename P = JotVfsProtocol>
struct groupOp : P {
    static constexpr const char* path = "jot/group";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& in, const std::vector<Shape>& others, Shape& out) {
        std::vector<Shape> all = in;
        all.insert(all.end(), others.begin(), others.end());

        out.geometry = std::nullopt;
        out.components = all;
        out.tf = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        out.add_tag("type", "group");
    }

    static std::vector<std::string> argument_keys() { return {"$in", "others"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/group"},
            {"aliases", {"jot/and"}}, // .and() is an alias for .group()
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"others", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}}}, {"others", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void group_init() {
    Processor::register_op<GroupOp<>, Shape, std::vector<Shape>>("jot/Group");
    Processor::register_op<groupOp<>, Shape, std::vector<Shape>, std::vector<Shape>>("jot/group");
}

} // namespace geo
} // namespace jotcad
