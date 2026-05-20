#pragma once
#include "protocols.h"
#include "processor.h"
#include "matrix.h"
#include "boolean/engine.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct DisjointOp : P {
    static constexpr const char* path = "jot/disjoint";

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in;
        boolean::Engine::deep_disjoint(vfs, out, Matrix::identity());
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return { 
            {"path", "jot/disjoint"}, 
            {"inputs", {
                {"$in", {{"type", "jot:shape"}}}
            }},
            {"arguments", nlohmann::json::array()}, 
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}} 
        };
    }
};

static void disjoint_init(fs::VFSNode* vfs) {
    Processor::register_op<DisjointOp<>, Shape>(vfs, "jot/disjoint");
}

} // namespace geo
} // namespace jotcad
