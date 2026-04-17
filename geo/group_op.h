#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "jot/group";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& sources, Shape& out) {
        Geometry combined;
        for (const auto& s : sources) {
            auto geo_selector = s.geometry;
            auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
                geo_selector.path, 
                geo_selector.parameters
            });
            Geometry g; g.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
            
            // Basic geometry combine (this is a logical grouping usually, but we can flatten for some ops)
            // For true CAD grouping, we just keep them as separate shapes in the parameter list.
        }

        out.geometry = {"jot/group", nlohmann::json::object()};
        nlohmann::json items = nlohmann::json::array();
        for (const auto& s : sources) items.push_back(s.to_json());
        out.geometry.parameters["items"] = items;
        out.add_tag("type", "group");
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shapes"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void group_init() {
    Processor::register_op<GroupOp<>, std::vector<Shape>>();
}

} // namespace geo
} // namespace jotcad
