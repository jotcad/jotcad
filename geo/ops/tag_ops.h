#pragma once
#include "protocols.h"
#include "processor.h"
#include "matcher.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct SetOp : P {
    static constexpr const char* path = "jot/set";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& key, const typename P::json& value) {
        Shape out = in;
        out.add_tag(key, value);
        vfs->write(fulfilling.with_output("$out"), out);
    }
    static std::vector<std::string> argument_keys() { return {"$in", "key", "value"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/set"},
            {"description", "Attaches metadata (a tag) to a shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "key"}, {"type", "string"}},
                {{"name", "value"}, {"type", "any"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct HasOp : P {
    static constexpr const char* path = "jot/has";

    static std::optional<Shape> has_recursive(const Shape& in, const std::string& key, const std::optional<typename P::json>& value, bool in_item = false) {
        fs::Selector sel("jot/has");
        sel.parameters["key"] = key;
        if (value.has_value()) sel.parameters["value"] = value.value();
        
        bool self_matches = Matcher::matches(in, sel);
        
        bool is_current_item = (in.tags.is_object() && in.tags.contains("type") && in.tags.at("type") == "item");
        if (is_current_item && in_item) {
            if (self_matches) {
                return in;
            }
            return std::nullopt;
        }

        bool next_in_item = in_item || is_current_item;

        std::vector<Shape> kept_children;
        for (const auto& c : in.components) {
            auto res = has_recursive(c, key, value, next_in_item);
            if (res) kept_children.push_back(*res);
        }

        if (self_matches || !kept_children.empty()) {
            Shape out = in;
            out.components = kept_children;
            return out;
        }
        return std::nullopt;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& key, std::optional<typename P::json> value) {
        auto res = has_recursive(in, key, value);
        if (res) {
            vfs->write(fulfilling.with_output("$out"), *res);
        } else {
            Shape out;
            out.tf = in.tf;
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "key", "value"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/has"},
            {"description", "Filters the shape tree, returning only components that have the specified tag (and optionally a matching value)."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "key"}, {"type", "string"}},
                {{"name", "value"}, {"type", "any"}, {"optional", true}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct GetOp : P {
    static constexpr const char* path = "jot/get";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& key) {
        if (in.tags.contains(key)) {
            vfs->write(fulfilling.with_output("$out"), in.tags[key]);
        } else {
            vfs->write(fulfilling.with_output("$out"), nlohmann::json(nullptr));
        }
    }
    static std::vector<std::string> argument_keys() { return {"$in", "key"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/get"},
            {"description", "Retrieves metadata (a tag) from a shape."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "key"}, {"type", "string"}}
            })},
            {"outputs", {{"$out", {{"type", "any"}}}}}
        };
    }
};

template <typename P = JotVfsProtocol>
struct InItemOp : P {
    static constexpr const char* path = "jot/inItem";

    static std::optional<Shape> in_item_recursive(const Shape& in, const std::string& name) {
        if (in.tags.is_object() && in.tags.contains("type") && in.tags.at("type") == "item" && 
            in.tags.contains("item:name") && in.tags.at("item:name") == name) {
            return in; 
        }

        std::vector<Shape> kept_children;
        for (const auto& c : in.components) {
            auto res = in_item_recursive(c, name);
            if (res) kept_children.push_back(*res);
        }

        if (!kept_children.empty()) {
            Shape out = in;
            out.components = kept_children;
            return out;
        }
        return std::nullopt;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& name) {
        auto res = in_item_recursive(in, name);
        if (res) {
            vfs->write(fulfilling.with_output("$out"), *res);
        } else {
            Shape out;
            out.tf = in.tf;
            out.add_tag("type", "group");
            vfs->write(fulfilling.with_output("$out"), out);
        }
    }

    static std::vector<std::string> argument_keys() { return {"$in", "name"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/inItem"},
            {"description", "Extracts the named rigid item from the assembly, preserving all its children and stopping at its boundary."},
            {"inputs", {{"$in", {{"type", "jot:shape"}}}}},
            {"arguments", json::array({
                {{"name", "name"}, {"type", "string"}}
            })},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

inline void tag_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<SetOp<>, Shape, std::string, nlohmann::json>(vfs, "jot/set");
    Processor::register_op<GetOp<>, Shape, std::string>(vfs, "jot/get");
    Processor::register_op<HasOp<>, Shape, std::string, std::optional<nlohmann::json>>(vfs, "jot/has");
    Processor::register_op<InItemOp<>, Shape, std::string>(vfs, "jot/inItem");
}

} // namespace geo
} // namespace jotcad
