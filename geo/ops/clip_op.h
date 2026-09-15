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
        Shape result = boolean::Engine::clip(vfs, in, tools);
        vfs->write(fulfilling.with_output("$out"), result);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/clip"}, 
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

static void clip_init(fs::VFSNode* vfs) {
    Processor::register_op<ClipOp<>, Shape, std::vector<Shape>>(vfs, "jot/clip");
}

} // namespace geo
} // namespace jotcad
