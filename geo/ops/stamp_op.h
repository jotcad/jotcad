#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct StampOp : P {
    static constexpr const char* path = "jot/stamp";

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

        // We pass stamp=true to force the "membrane effect" (grafting tool boundary)
        boolean::Engine::recursive_subtract(vfs, out, Matrix::identity(), tool_nodes, false, true);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/stamp"}, 
            {"description", "Topological embossing: grafts the boundary of 3D tools onto the subject's surface."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array({ 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void stamp_init(fs::VFSNode* vfs) {
    Processor::register_op<StampOp<>, Shape, std::vector<Shape>>(vfs, "jot/stamp");
}

} // namespace geo
} // namespace jotcad
