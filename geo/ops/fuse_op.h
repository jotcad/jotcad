#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FuseOp : P {
    static constexpr const char* path = "jot/fuse";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<Shape>& tools) {
        std::vector<Shape> all_shapes;
        all_shapes.reserve(1 + tools.size());
        all_shapes.push_back(in);
        all_shapes.insert(all_shapes.end(), tools.begin(), tools.end());
        Shape out = boolean::Engine::fuse(vfs, all_shapes);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/fuse"}, 
            {"inputs", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to fuse."}}}}},
            {"arguments", nlohmann::json::array({ 
                {{"name", "tools"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

template <typename P = JotVfsProtocol>
struct FusePrimitiveOp : P {
    static constexpr const char* path = "jot/Fuse";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const std::vector<Shape>& shapes) {
        Shape out = boolean::Engine::fuse(vfs, shapes);
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"shapes"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/Fuse"},
            {"dsl_name", "Fuse"},
            {"role", "constructor"},
            {"description", "Fuses multiple shapes into a single union shape."},
            {"synonyms", {"Union", "Combine"}},
            {"inputs", nlohmann::json::object()}, 
            {"arguments", nlohmann::json::array({ 
                {{"name", "shapes"}, {"type", "jot:shapes"}, {"default", nlohmann::json::array()}, {"description", "The list of shapes to fuse."}}
            })}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void fuse_init(fs::VFSNode* vfs) {
    Processor::register_op<FuseOp<>, Shape, std::vector<Shape>>(vfs, "jot/fuse");
    Processor::register_op<FusePrimitiveOp<>, std::vector<Shape>>(vfs, "jot/Fuse");
}

} // namespace geo
} // namespace jotcad
