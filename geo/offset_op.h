#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/offset.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter) {
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res = geo;
        applyOffset(res, diameter);
        Shape out = in;
        out.geometry = vfs->write<Geometry>(res);
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset"},
            {"description", "Creates a Minkowski offset."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"diameter", {{"type", "number"}, {"default", 1.0}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct OffsetClosureOp : P {
    static constexpr const char* path = "jot/offset/closure";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter, bool closure) {
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry expanded = geo;
        applyOffset(expanded, std::abs(diameter));
        Geometry closed = expanded;
        applyOffset(closed, -std::abs(diameter));
        Shape out = in;
        out.geometry = vfs->write<Geometry>(closed);
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter", "closure"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset/closure"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"diameter", {{"type", "number"}, {"default", 1.0}}},
                {"closure", {{"type", "boolean"}, {"const", true}}}
            }}
        };
    }
};

static void offset_init() {
    Processor::register_op<OffsetOp<>, Shape, double>("jot/offset");
    Processor::register_op<OffsetClosureOp<>, Shape, double, bool>("jot/offset/closure");
}

} // namespace geo
} // namespace jotcad
