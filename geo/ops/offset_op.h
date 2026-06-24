#pragma once
#include "protocols.h"
#include "processor.h"
#include "offset.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";

    static void apply_offset_recursive(fs::VFSNode* vfs, Shape& s, double diameter) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            applyOffset(geo, FT(diameter));
            geo.triangulate();
            s.geometry = vfs->materialize<Geometry>(geo);
        }
        for (auto& child : s.components) {
            apply_offset_recursive(vfs, child, diameter);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter) {
        Shape out = in;
        apply_offset_recursive(vfs, out, diameter);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset"},
            {"description", "Creates a Minkowski offset. Recursively processes groups."},
            {"inputs", nlohmann::json::object({{"$in", {{"type", "jot:shape"}}}})},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 1.0}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct OffsetClosureOp : P {
    static constexpr const char* path = "jot/offset/closure";

    static void apply_closure_recursive(fs::VFSNode* vfs, Shape& s, double diameter) {
        if (s.geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(s.geometry.value());
            Geometry expanded = geo;
            applyOffset(expanded, FT(std::abs(diameter)));
            Geometry closed = expanded;
            applyOffset(closed, FT(-std::abs(diameter)));
            closed.triangulate();
            s.geometry = vfs->materialize<Geometry>(closed);
        }
        for (auto& child : s.components) {
            apply_closure_recursive(vfs, child, diameter);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter, bool closure) {
        Shape out = in;
        apply_closure_recursive(vfs, out, diameter);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "diameter", "closure"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/offset/closure"},
            {"description", "Applies an outward then inward offset to close gaps. Recursively processes groups."},
            {"inputs", nlohmann::json::object({{"$in", {{"type", "jot:shape"}}}})},
            {"arguments", nlohmann::json::array({
                {{"name", "diameter"}, {"type", "jot:number"}, {"default", 1.0}},
                {{"name", "closure"}, {"type", "jot:boolean"}, {"const", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void offset_init(fs::VFSNode* vfs) {
    Processor::register_op<OffsetOp<>, Shape, double>(vfs, "jot/offset");
    Processor::register_op<OffsetClosureOp<>, Shape, double, bool>(vfs, "jot/offset/closure");
}

} // namespace geo
} // namespace jotcad
