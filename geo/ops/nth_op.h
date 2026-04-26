#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct NthOp : P {
    static constexpr const char* path = "jot/nth";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, int index) {
        if (index >= 0 && index < (int)in.components.size()) {
            vfs->write(fulfilling.with_output("$out"), in.components[index]);
        } else {
            throw std::runtime_error("[NthOp] Index " + std::to_string(index) + " out of bounds (size " + std::to_string(in.components.size()) + ")");
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "index"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/nth"},
            {"description", "Selects the N-th component shape from a group."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}, {"affiliate", "$out"}}},
                {"index", {{"type", "integer"}, {"default", 0}}}
            }},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void nth_init(fs::VFSNode* vfs) {
    Processor::register_op<NthOp<>, Shape, int>(vfs, "jot/nth");
}

} // namespace geo
} // namespace jotcad
