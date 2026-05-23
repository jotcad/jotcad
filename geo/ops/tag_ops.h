#pragma once
#include "protocols.h"
#include "processor.h"
#include "matcher.h"

namespace jotcad {
namespace geo {


inline Shape MaybeGroup(std::vector<Shape>& results) {
  if (results.size() == 1) {
    return results[0];
  } else {
    Shape group;
    group.components = results;
    group.add_tag("type", "group");
    return group;
  }
}

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

inline bool wildcard_match(const std::string& pattern, const std::string& str) {
    if (pattern == "*") return true;
    const char* pat = pattern.c_str();
    const char* s = str.c_str();
    const char* cp = nullptr;
    const char* mp = nullptr;
    while (*s) {
        if (*pat == '*') {
            if (!*++pat) return true;
            mp = pat;
            cp = s + 1;
        } else if (*pat == *s || *pat == '?') {
            pat++;
            s++;
        } else if (mp) {
            pat = mp;
            s = cp++;
        } else {
            return false;
        }
    }
    while (*pat == '*') {
        pat++;
    }
    return !*pat;
}

template <typename P = JotVfsProtocol>
struct HasOp : P {
    static constexpr const char* path = "jot/has";

    static void has_recursive(const Shape& in, fs::Selector& sel, std::vector<Shape>& results) {
        if (Matcher::matches(in, sel)) {
            results.push_back(in);
        }

        if (in.tags.is_object() && in.tags.contains("type") && in.tags.at("type") == "item") {
            return;
        }

        for (const auto& c : in.components) {
            has_recursive(c, sel, results);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& key, std::optional<typename P::json> value) {
        fs::Selector sel("jot/has");
        sel.parameters["key"] = key;
        if (value.has_value()) {
            sel.parameters["value"] = value.value();
        }
        std::vector<Shape> results;
        has_recursive(in, sel, results);
        vfs->write(fulfilling.with_output("$out"), MaybeGroup(results));
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
        if (in.tags.is_object() && in.tags.contains(key)) {
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

    static void in_item_recursive(const Shape& in, const std::string& name, std::vector<Shape>& results) {
        if (in.tags.is_object() && in.tags.contains("type") && in.tags.at("type") == "item" && 
            in.tags.contains("item:name") && in.tags.at("item:name").is_string()) {
            std::string item_name = in.tags.at("item:name").get<std::string>();
            if (wildcard_match(name, item_name)) {
                for (const auto& c : in.components) {
                    results.push_back(c);
                }
                return;
            }
        }

        for (const auto& c : in.components) {
            in_item_recursive(c, name, results);
        }
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in, const std::string& name) {
        std::vector<Shape> results;
        in_item_recursive(in, name, results);
        vfs->write(fulfilling.with_output("$out"), MaybeGroup(results));
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
