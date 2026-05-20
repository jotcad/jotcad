#pragma once
#include "protocols.h"
#include "processor.h"

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
            {"arguments", {
                {{"name", "key"}, {"type", "string"}},
                {{"name", "value"}, {"type", "any"}}
            }},
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
            {"arguments", {
                {{"name", "key"}, {"type", "string"}}
            }},
            {"outputs", {{"$out", {{"type", "any"}}}}}
        };
    }
};

inline void tag_ops_init(fs::VFSNode* vfs) {
    Processor::register_op<SetOp<>, Shape, std::string, nlohmann::json>(vfs, "jot/set");
    Processor::register_op<GetOp<>, Shape, std::string>(vfs, "jot/get");
}

} // namespace geo
} // namespace jotcad
