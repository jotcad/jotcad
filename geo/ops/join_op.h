#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct JoinOp : P {
    static constexpr const char* path = "jot/join";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        Shape out = boolean::Engine::join(vfs, in, tools);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/join"}, 
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to join to."}}}}},
            {"arguments", json::array({ 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void join_init(fs::VFSNode* vfs) {
    Processor::register_op<JoinOp<>, Shape, std::vector<Shape>>(vfs, "jot/join");
}

} // namespace geo
} // namespace jotcad
