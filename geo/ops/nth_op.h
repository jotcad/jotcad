#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct NthOp : P {
    static constexpr const char* path = "jot/nth";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::vector<double>& indices) {
        if (indices.size() == 1) {
            int index = (int)indices[0];
            if (index >= 0 && index < (int)in.components.size()) {
                vfs->write(fulfilling.with_output("$out"), in.components[index]);
            } else {
                throw std::runtime_error("[NthOp] Index " + std::to_string(index) + " out of bounds. Components: " + std::to_string(in.components.size()));
            }
        } else {
            Shape out;
            for (double d : indices) {
                int index = (int)d;
                if (index >= 0 && index < (int)in.components.size()) {
                    out.components.push_back(in.components[index]);
                } else {
                    throw std::runtime_error("[NthOp] Index " + std::to_string(index) + " out of bounds. Components: " + std::to_string(in.components.size()));
                }
            }
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "index"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/nth"},
            {"description", "Selects the N-th component shape from a group."},
            {"inputs", nlohmann::json::object({{"$in", {{"type", "jot:shape"}}}})},
            {"arguments", nlohmann::json::array({
                {{"name", "index"}, {"type", "jot:numbers"}, {"default", {0}}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void nth_init(fs::VFSNode* vfs) {
    Processor::register_op<NthOp<>, Shape, std::vector<double>>(vfs, "jot/nth");
}

} // namespace geo
} // namespace jotcad
