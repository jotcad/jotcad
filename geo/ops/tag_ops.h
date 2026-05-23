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

    static std::optional<Shape> has_recursive(const Shape& in, const std::string& key, const std::optional<typename P::json>& value) {
        fs::Selector sel("jot/has");
        sel.parameters["key"] = key;
        if (value.has_value()) sel.parameters["value"] = value.value();
        
        bool self_matches = Matcher::matches(in, sel);
        
        std::vector<Shape> kept_children;
        for (const auto& c : in.components) {
            auto res = has_recursive(c, key, value);
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

inline void tag_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<SetOp<>, Shape, std::string, nlohmann::json>(vfs, "jot/set");
    Processor::register_op<GetOp<>, Shape, std::string>(vfs, "jot/get");
    Processor::register_op<HasOp<>, Shape, std::string, std::optional<nlohmann::json>>(vfs, "jot/has");
}

} // namespace geo
} // namespace jotcad
