#pragma once
#include "protocols.h"
#include "processor.h"
#include "matcher.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct KeepOp : P {
    static constexpr const char* path = "jot/keep";

    static bool is_exact_match(const Shape& a, const Shape& b) {
        if (a.geometry.has_value() != b.geometry.has_value()) return false;
        if (a.geometry.has_value()) {
            if (a.geometry.value() != b.geometry.value()) return false;
        }
        if (a.tags != b.tags) return false;
        if (a.components.size() != b.components.size()) return false;
        for (size_t i = 0; i < a.components.size(); ++i) {
            if (!is_exact_match(a.components[i], b.components[i])) return false;
        }
        return true;
    }

    static bool matches_tool_tree(const Shape& s, const Shape& tool) {
        if (is_exact_match(s, tool)) return true;
        for (const auto& child : tool.components) {
            if (matches_tool_tree(s, child)) return true;
        }
        return false;
    }

    static std::optional<Shape> keep_recursive(const Shape& in, const Shape& tool) {
        bool self_matches = matches_tool_tree(in, tool);
        
        std::vector<Shape> kept_children;
        for (const auto& c : in.components) {
            auto res = keep_recursive(c, tool);
            if (res) kept_children.push_back(*res);
        }

        if (self_matches || !kept_children.empty()) {
            Shape out = in;
            out.components = kept_children;
            return out;
        }
        return std::nullopt;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& selector) {
        auto res = keep_recursive(in, selector);
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
            {"path", "jot/keep"},
            {"description", "Prunes the subject tree, retaining only the components that match the provided shape or set of shapes."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "selector"}, {"type", "jot:shape"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct DropOp : P {
    static constexpr const char* path = "jot/drop";

    static std::optional<Shape> drop_recursive(const Shape& in, const Shape& tool) {
        if (KeepOp<P>::matches_tool_tree(in, tool)) return std::nullopt;

        Shape out = in;
        out.components.clear();
        for (const auto& c : in.components) {
            auto res = drop_recursive(c, tool);
            if (res) out.components.push_back(*res);
        }
        return out;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const Shape& selector) {
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
            {"description", "Removes all components from the tree that match the provided shape or set of shapes."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "selector"}, {"type", "jot:shape"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void filter_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<KeepOp<>, Shape, Shape>(vfs, "jot/keep");
    Processor::register_op<DropOp<>, Shape, Shape>(vfs, "jot/drop");
}

} // namespace geo
} // namespace jotcad
