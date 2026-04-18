#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/offset.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, double distance, Shape& out) {
        // 1. Resolve Input Geometry
        Geometry geo = vfs->template read<Geometry>({
            in.geometry.path, 
            in.geometry.parameters
        });
        
        // 2. Perform Real Geometric Offset (Minkowski)
        applyOffset(geo, distance);

        // 3. Sink to Mesh (Deduplicated) and Return
        out = in;
        out.geometry = vfs->write_geometry(geo);
        out.add_tag("operation", "offset");
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
    Processor::register_op<OffsetOp<>, Shape, Shape, double>();
}

} // namespace geo
} // namespace jotcad
