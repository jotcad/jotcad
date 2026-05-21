#pragma once
#include "protocols.h"
#include "processor.h"
#include "matcher.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct KeepOp : P {
    static constexpr const char* path = "jot/keep";

    static std::optional<Shape> keep_recursive(const Shape& in, const fs::Selector& sel) {
        bool self_matches = Matcher::matches(in, sel);
        
        std::vector<Shape> kept_children;
        for (const auto& c : in.components) {
            auto res = keep_recursive(c, sel);
            if (res) kept_children.push_back(*res);
        }

        if (self_matches || !kept_children.empty()) {
            Shape out = in;
            out.components = kept_children;
            return out;
        }
        return std::nullopt;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const fs::Selector& selector) {
        auto res = keep_recursive(in, selector);
        if (res) {
            vfs->write(fulfilling.with_output("$out"), *res);
        } else {
            // Return empty group if nothing matches
            Shape out;
            out.tf = in.tf;
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "selector"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/keep"},
            {"description", "Prunes the subject tree, retaining only the components that match the provided selector."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "selector"}, {"type", "jot:selector"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct DropOp : P {
    static constexpr const char* path = "jot/drop";

    static std::optional<Shape> drop_recursive(const Shape& in, const fs::Selector& sel) {
        if (Matcher::matches(in, sel)) return std::nullopt;

        Shape out = in;
        out.components.clear();
        for (const auto& c : in.components) {
            auto res = drop_recursive(c, sel);
            if (res) out.components.push_back(*res);
        }
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const fs::Selector& selector) {
        auto res = drop_recursive(in, selector);
        if (res) {
            vfs->write(fulfilling.with_output("$out"), *res);
        } else {
            Shape out;
            out.tf = in.tf;
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "selector"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/drop"},
            {"description", "Removes all components from the tree that match the provided selector."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "selector"}, {"type", "jot:selector"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void filter_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<KeepOp<>, Shape, fs::Selector>(vfs, "jot/keep");
    Processor::register_op<DropOp<>, Shape, fs::Selector>(vfs, "jot/drop");
}

} // namespace geo
} // namespace jotcad
