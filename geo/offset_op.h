#pragma once
#include "impl/protocols.h"
#include "impl/geometry.h"
#include "impl/offset.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, double distance, Shape& out) {
        if (!in.geometry.has_value()) {
            out = in;
            return;
        }

        Geometry geo = vfs->template read<Geometry>({in.geometry->path, in.geometry->parameters});
        applyOffset(geo, FT(distance));
        
        out = in;
        out.geometry = vfs->write_geometry(geo);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "distance"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/offset"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"distance", {{"type", "jot:number"}, {"default", 1.0}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"alias", "$in"}}}}}
        };
    }
};

static void offset_init() {
    Processor::register_op<OffsetOp<>, Shape, Shape, double>("jot/offset");
}

} // namespace geo
} // namespace jotcad
