#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/points";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, Shape& out) {
        auto geo_selector = in.geometry;
        auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
            geo_selector.path, 
            geo_selector.parameters
        });
        
        Geometry geo; geo.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
        
        // Logical Transform: Return points
        nlohmann::json pts = nlohmann::json::array();
        for (const auto& v : geo.vertices) {
            pts.push_back({v.x, v.y, v.z});
        }

        out = in;
        out.geometry = {"op/points", {{"points", pts}}};
        out.add_tag("operation", "points");
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"$out", {{"type", "jot:shape"}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void points_init() {
    Processor::register_op<PointsOp<>, Shape, Shape>();
}

} // namespace geo
} // namespace jotcad
