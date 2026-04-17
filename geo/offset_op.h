#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, double distance, Shape& out) {
        out = in;
        out.geometry.parameters["distance"] = distance;
        out.add_tag("operation", "offset");
        if (in.tags.value("plane", "") == "Z0") out.add_tag("plane", "Z0");
    }

    static std::vector<std::string> argument_keys() { return {"$in", "distance"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"distance", {{"type", "jot:number"}, {"default", 1.0}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void offset_init() {
    Processor::register_op<OffsetOp<>, Shape, double>();
}

} // namespace geo
} // namespace jotcad
