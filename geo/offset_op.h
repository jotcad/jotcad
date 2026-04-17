#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "op/offset";

    /**
     * THE CLEAN ENTRY POINT:
     * - 'in' is AUTOMATICALLY resolved and parsed into a Shape.
     * - 'radius' is AUTOMATICALLY decoded.
     * - 'out' is a mutable reference to the $out port.
     */
    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, double radius, Shape& out) {
        // 1. Pure Semantic Logic
        out = in; // Transform Pass-through
        out.geometry.parameters["radius"] = radius;
        out.add_tag("operation", "offset");

        // 2. Maintain Z0 tag
        if (in.tags.value("plane", "") == "Z0") {
            out.add_tag("plane", "Z0");
        }
    }

    // Maps JSON keys to execute() parameters in order
    static std::vector<std::string> argument_keys() { return {"$in", "radius", "$out"}; }

    // Logic Bridge (Until Processor::dispatch is fully generic)
    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<Shape>(vfs, "$in", params, schema(), stack);
        auto radius = Processor::decode<double>(vfs, "radius", params, schema(), stack);
        
        Shape out;
        execute(vfs, in, radius, out);
        
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"radius", {{"type", "jot:number"}, {"default", 1.0}}}
            }},
            {"inputs", {
                {"$in", {{"type", "shape"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}}}
            }}
        };
    }
};

static void offset_init() {
    Processor::register_op<OffsetOp<>>();
}

} // namespace geo
} // namespace jotcad
