#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct ClipOp : P {
    static constexpr const char* path = "jot/clip";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        Shape out = in;
        if (tools.empty()) {
            vfs->write(fulfilling.with_output("$out"), out);
            return;
        }

        std::vector<boolean::Engine::ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            boolean::Engine::collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        boolean::Engine::recursive_intersect(vfs, out, Matrix::identity(), tool_nodes);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/clip"}, 
            {"arguments", { 
                {{"name", "$in"}, {"type", "jot:shape"}, {"affiliate", "$out"}}, 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            }}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void clip_init(fs::VFSNode* vfs) {
    Processor::register_op<ClipOp<>, Shape, std::vector<Shape>>(vfs, "jot/clip");
}

} // namespace geo
} // namespace jotcad
