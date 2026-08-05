#pragma once
#include "protocols.h"
#include "processor.h"
#include "render/png_op.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PngOp : P {
    static constexpr const char* path = "jot/png";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        // Generate the PNG directly to the primary '$out' port
        PngOpImpl::execute(vfs, fulfilling, in);
    }

    static std::vector<std::string> argument_keys() { return {"$in", "ax", "ay", "width", "height", "wireframe"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/png"},
            {"description", "Generates a PNG thumbnail for the input shape, with optional wireframe edge overlay."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "ax"}, {"type", "jot:number"}, {"optional", true}, {"default", 0.0}},
                {{"name", "ay"}, {"type", "jot:number"}, {"optional", true}, {"default", 0.0}},
                {{"name", "width"}, {"type", "jot:number"}, {"optional", true}, {"default", 256}},
                {{"name", "height"}, {"type", "jot:number"}, {"optional", true}, {"default", 256}},
                {{"name", "wireframe"}, {"type", "jot:number"}, {"optional", true}, {"default", 0.0}}
            })},
            {"outputs", {
                {"$out", {{"type", "file"}, {"mimeType", "image/png"}, {"description", "The generated PNG thumbnail."}}}
            }}
        };
    }
};

inline void png_init(fs::VFSNode* vfs) {
    Processor::register_op<PngOp<>, Shape>(vfs, "jot/png");
}

} // namespace geo
} // namespace jotcad
