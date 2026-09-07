#pragma once
#include "protocols.h"
#include "processor.h"
#include "offset.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OffsetOp : P {
    static constexpr const char* path = "jot/offset";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter) {
        Shape out = in.map([&](Shape node) {
            if (node.has_positive_geometry()) {
                Geometry geo = vfs->read<Geometry>(node.geometry.value());
                applyOffset(geo, FT(diameter));
                geo.triangulate();
                node.geometry = vfs->materialize<Geometry>(geo);
            }
            return node;
        });
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

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, double diameter, bool closure) {
        Shape out = in.map([&](Shape node) {
            if (node.has_positive_geometry()) {
                Geometry geo = vfs->read<Geometry>(node.geometry.value());
                Geometry expanded = geo;
                applyOffset(expanded, FT(std::abs(diameter)));
                Geometry closed = expanded;
                applyOffset(closed, FT(-std::abs(diameter)));
                closed.triangulate();
                node.geometry = vfs->materialize<Geometry>(closed);
            }
            return node;
        });
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
