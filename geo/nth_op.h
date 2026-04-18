#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct NthOp : P {
    static constexpr const char* path = "jot/nth";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& sources, double index, Shape& out) {
        if (sources.empty()) return;
        
        int i = (int)index;
        if (i < 0) i = 0;
        if (i >= (int)sources.size()) i = (int)sources.size() - 1;
        
        out = sources[i];
    }

    static std::vector<std::string> argument_keys() { return {"$in", "index"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"index", {{"type", "jot:number"}, {"default", 0}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void nth_init() {
    Processor::register_op<NthOp<>, Shape, std::vector<Shape>, double>();
}

} // namespace geo
} // namespace jotcad
