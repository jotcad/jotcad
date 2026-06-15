#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct CutOp : P {
    static constexpr const char* path = "jot/cut";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools, bool open = false) {
        Shape result = in;
        if (tools.empty()) {
            vfs->write(fulfilling.with_output("$out"), result);
            return;
        }

        std::vector<boolean::Engine::ToolNode> tool_nodes;
        for (const auto& tool : tools) {
            boolean::Engine::collect_tool_geometry(vfs, tool, Matrix::identity(), tool_nodes);
        }

        boolean::Engine::recursive_subtract(vfs, result, Matrix::identity(), tool_nodes, open);
        
        for (const auto& tool : tools) {
            result.components.push_back(Shape::make_ghost(tool));
        }

        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools", "open"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/cut"}, 
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array({ 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}, 
                {{"name", "open"}, {"type", "jot:boolean"}, {"default", false}} 
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void cut_init(fs::VFSNode* vfs) {
    Processor::register_op<CutOp<>, Shape, std::vector<Shape>, bool>(vfs, "jot/cut");
}

} // namespace geo
} // namespace jotcad
