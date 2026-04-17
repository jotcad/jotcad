#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct OutlineOp : P {
    static constexpr const char* path = "jot/outline";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, Shape& out) {
        auto geo_selector = in.geometry;
        auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
            geo_selector.path, 
            geo_selector.parameters
        });
        
        Geometry geo; geo.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
        
        Geometry outline;
        outline.vertices = geo.vertices;
        for (const auto& face : geo.faces) {
            for (const auto& loop : face.loops) {
                Geometry::Face f; f.loops.push_back(loop);
                outline.faces.push_back(f);
            }
        }

        out = in;
        out.geometry = Shape::from_json(P::json::parse(P::write_shape(vfs, {{"op","outline"}}, outline, {{"type","outline"}}))).geometry;
        out.add_tag("operation", "outline");
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

static void outline_init() {
    Processor::register_op<OutlineOp<>, Shape>();
}

} // namespace geo
} // namespace jotcad
