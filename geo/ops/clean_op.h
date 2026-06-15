#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct CleanOp : P {
    static constexpr const char* path = "jot/clean";

    static std::optional<Shape> clean_recursive(const Shape& in, const std::string& target_role) {
        if (in.role() == target_role) return std::nullopt;

        Shape out = in;
        out.components.clear();
        for (const auto& c : in.components) {
            auto res = clean_recursive(c, target_role);
            if (res) out.components.push_back(*res);
        }
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, std::string role) {
        auto res = clean_recursive(in, role);
        if (res) {
            vfs->write(fulfilling.with_output("$out"), *res);
        } else {
            Shape out;
            out.tf = in.tf;
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "role"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/clean"},
            {"description", "Recursively removes all components with the specified role (default: ghost) from the shape tree."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", nlohmann::json::array({
                {{"name", "role"}, {"type", "jot:string"}, {"default", "ghost"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void clean_init(fs::VFSNode* vfs) {
    Processor::register_op<CleanOp<>, Shape, std::string>(vfs, "jot/clean");
}

} // namespace geo
} // namespace jotcad
