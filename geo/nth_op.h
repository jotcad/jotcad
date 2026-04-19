#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct NthOp : P {
    static constexpr const char* path = "jot/nth";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, int index) {
        if (index >= 0 && index < (int)in.components.size()) {
            vfs->write<Shape>(fulfilling, in.components[index]);
        } else {
            vfs->write<Shape>(fulfilling, in);
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "index"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/nth"},
            {"description", "Selects the N-th component shape from a group."},
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"index", {{"type", "integer"}, {"default", 0}}}
            }}
        };
    }
};

static void nth_init() {
    Processor::register_op<NthOp<>, Shape, int>("jot/nth");
}

} // namespace geo
} // namespace jotcad
