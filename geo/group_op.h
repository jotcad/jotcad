#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/geometry.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct GroupOp : P {
    static constexpr const char* path = "op/group";

    static void execute(jotcad::fs::VFSNode* vfs, const std::vector<Shape>& sources, Shape& out) {
        Geometry combined;
        for (const auto& s : sources) {
            auto geo_selector = s.geometry;
            auto geo_bytes = vfs->template read<std::vector<uint8_t>>({
                geo_selector.path, 
                geo_selector.parameters, 
                {} 
            });

            Geometry mesh;
            mesh.decode_text(std::string(geo_bytes.begin(), geo_bytes.end()));
            mesh.apply_tf(s.tf);

            int offset = (int)combined.vertices.size();
            combined.vertices.insert(combined.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
            for (auto p : mesh.points) combined.points.push_back(p + offset);
            for (auto seg : mesh.segments) combined.segments.push_back({seg[0] + offset, seg[1] + offset});
            for (auto t : mesh.triangles) combined.triangles.push_back({t[0] + offset, t[1] + offset, t[2] + offset});
            for (auto f : mesh.faces) {
                Geometry::Face nf;
                for (auto l : f.loops) {
                    std::vector<int> nl;
                    for (int i : l) nl.push_back(i + offset);
                    nf.loops.push_back(nl);
                }
                combined.faces.push_back(nf);
            }
        }

        auto shape_data = P::write_shape(vfs, {}, combined, {{"type", "group"}});
        out = Shape::from_json(nlohmann::json::parse(shape_data));
    }

    static std::vector<uint8_t> logic(jotcad::fs::VFSNode* vfs, const std::string& path, const typename P::json& params, const std::vector<std::string>& stack) {
        auto in = Processor::decode<std::vector<Shape>>(vfs, "$in", params, schema(), stack);
        Shape out;
        execute(vfs, in, out);
        return P::write_shape_obj(out);
    }

    static typename P::json schema() {
        return {
            {"arguments", {{"$in", {{"type", "jot:shapes"}}}}},
            {"inputs", {{"$in", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "jot:shape"}}}}}
        };
    }
};

static void group_init() {
    Processor::register_op<GroupOp<>>();
}

} // namespace geo
} // namespace jotcad
