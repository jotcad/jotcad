#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "jot/group";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
        Shape out;
        out.components = shapes;
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"shapes"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/group"},
            {"description", "Combines multiple shapes into a single hierarchical group."},
            {"arguments", {{"shapes", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "The shapes to group together."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting group shape."}}}}}
        };
    }
};

static void group_init() {
    Processor::register_op<GroupOp<>, std::vector<Shape>>("jot/group");
}

} // namespace geo
} // namespace jotcad
