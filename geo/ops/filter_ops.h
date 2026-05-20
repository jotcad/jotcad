#pragma once
#include "protocols.h"
#include "processor.h"

namespace jotcad {
namespace geo {

struct Matcher {
    static bool matches(const Shape& s, const fs::Selector& sel) {
        if (sel.path == "jot/has") {
            std::string key = sel.parameters.value("key", "");
            if (key.empty()) return false;
            if (!s.tags.contains(key)) return false;
            if (sel.parameters.contains("value") && !sel.parameters.at("value").is_null()) {
                auto val = sel.parameters.at("value");
                auto tag_val = s.tags.at(key);
                
                // Flexible comparison (string vs number)
                if (val.is_number() && tag_val.is_string()) {
                    try { return std::stod(tag_val.get<std::string>()) == val.get<double>(); } catch(...) { return false; }
                }
                if (val.is_string() && tag_val.is_number()) {
                    try { return std::stod(val.get<std::string>()) == tag_val.get<double>(); } catch(...) { return false; }
                }
                
                return tag_val == val;
            }
            return true;
        }
        
        // Support for nth(i)
        if (sel.path == "jot/nth") {
            // This is harder because Matcher doesn't know its index in the parent.
            // Selection by index should probably be handled by the parent during traversal.
        }

        return false;
    }
};

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
            {"arguments", {
                {{"name", "selector"}, {"type", "jot:selector"}}
            }},
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
            {"arguments", {
                {{"name", "selector"}, {"type", "jot:selector"}}
            }},
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
