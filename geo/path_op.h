#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct LinkOp : P {
    static constexpr const char* path = "jot/link";
    static void execute(jotcad::fs::VFSNode* vfs, const Shape& a, const Shape& b, Shape& out) {
        out.geometry = std::nullopt; // Clear geometry
        out.components = {a, b};
        out.add_tag("type", "link");
    }
    static std::vector<std::string> argument_keys() { return {"$a", "$b"}; }
    static typename P::json schema() {
        return {{"arguments", {{"$a", {{"type", "jot:shape"}}}, {"$b", {{"type", "jot:shape"}}}, {"$out", {{"type", "jot:shape"}}}}}, {"inputs", {{"$a", {{"type", "shape"}}}, {"$b", {{"type", "shape"}}}}}, {"outputs", {{"$out", {{"type", "shape"}}}}}};
    }
};

template <typename P = JotVfsProtocol>
struct LoopOp : P {
    static constexpr const char* path = "jot/loop";
    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& segments, Shape& out) {
        out.geometry = std::nullopt;
        out.components = segments;
        out.add_tag("type", "loop");
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {{"arguments", {{"$in", {{"type", "jot:shapes"}}}, {"$out", {{"type", "jot:shape"}}}}}, {"inputs", {{"$in", {{"type", "shapes"}}}}}, {"outputs", {{"$out", {{"type", "shape"}}}}}};
    }
};

static void path_init() {
    Processor::register_op<LinkOp<>, Shape, Shape, Shape>();
    Processor::register_op<LoopOp<>, Shape, std::vector<Shape>>();
}

} // namespace geo
} // namespace jotcad
