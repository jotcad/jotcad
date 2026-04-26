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
        // Pass-through the shape (primary result)
        vfs->write(fulfilling.with_output("$out"), in);
        
        // Generate the PNG (secondary result into the 'file' port)
        PngOpImpl::execute(vfs, fulfilling, in);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"path", "jot/png"},
            {"description", "Generates a PNG thumbnail for the input shape and stores it in the 'file' port."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}}
            }},
            {"outputs", {
                {"$out", {{"type", "jot:shape"}, {"description", "The input shape (pass-through)."}}},
                {"file", {{"type", "mime:png"}, {"description", "The generated PNG thumbnail."}}}
            }}
        };
    }
};

inline void png_init(fs::VFSNode* vfs) {
    Processor::register_op<PngOp<>, Shape>(vfs, "jot/png");
}

} // namespace geo
} // namespace jotcad
