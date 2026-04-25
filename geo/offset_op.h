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
        if (!in.geometry.has_value()) {
            throw std::runtime_error("jot/offset: Input shape has no geometry");
        }
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        Geometry res = geo;
        applyOffset(res, diameter);
        Shape out = in;
        out.geometry = vfs->write_anonymous<Geometry>(res);
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset"},
            {"description", "Creates a Minkowski offset."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
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
        out.geometry = vfs->write_anonymous<Geometry>(closed);
        vfs->write<Shape>(fulfilling, out, "$out");
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter", "closure"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset/closure"},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"diameter", {{"type", "number"}, {"default", 1.0}}},
                {"closure", {{"type", "boolean"}, {"const", true}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void offset_init(fs::VFSNode* vfs) {
    Processor::register_op<OffsetOp<>, Shape, double>(vfs, "jot/offset");
    Processor::register_op<OffsetClosureOp<>, Shape, double, bool>(vfs, "jot/offset/closure");
}

} // namespace geo
} // namespace jotcad
