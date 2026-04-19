#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct LinkOp : P {
    static constexpr const char* path = "jot/link";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& a, const Shape& b) {
        Shape out;
        out.geometry = std::nullopt;
        out.components = {a, b};
        out.add_tag("type", "link");
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$a", "$b"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/link"},
            {"description", "Creates a semantic link between two shapes, establishing a topological connection."},
            {"arguments", {
                {"$a", {{"type", "jot:shape"}, {"description", "The first shape to be linked."}}},
                {"$b", {{"type", "jot:shape"}, {"description", "The second shape to be linked."}}}
            }},
            {"inputs", {
                {"$a", {{"type", "shape"}, {"description", "The first shape."}}}, 
                {"$b", {{"type", "shape"}, {"description", "The second shape."}}}
            }},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "A group containing both linked shapes."}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct LoopOp : P {
    static constexpr const char* path = "jot/loop";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& segments) {
        Shape out;
        out.geometry = std::nullopt;
        out.components = segments;
        out.add_tag("type", "loop");
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/loop"},
            {"description", "Closes a sequence of connected segments into a topological loop."},
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}, {"description", "The sequence of segments to form into a loop."}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}, {"description", "The sequence of segments."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting loop shape."}}}}}
        };
    }
};

static void path_init() {
    Processor::register_op<LinkOp<>, Shape, Shape>("jot/link");
    Processor::register_op<LoopOp<>, std::vector<Shape>>("jot/loop");
}

} // namespace geo
} // namespace jotcad
