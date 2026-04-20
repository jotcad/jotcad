#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct PointsOp : P {
    static constexpr const char* path = "jot/points";
    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        if (!in.geometry.has_value()) {
            throw std::runtime_error("jot/points: Input shape has no geometry");
        }
        
        Geometry geo = vfs->read<Geometry>(in.geometry.value());
        std::cout << "[PointsOp] Exploding " << geo.vertices.size() << " vertices" << std::endl;
        
        // 1. Create a canonical single point at origin
        Geometry pt_geo;
        pt_geo.vertices.push_back({FT(0), FT(0), FT(0)});
        pt_geo.faces.push_back({{{0}}});
        fs::Selector pt_cid = vfs->write<Geometry>(pt_geo);

        // 2. Explode vertices into separate component shapes
        std::vector<Shape> components;
        for (const auto& v : geo.vertices) {
            Shape s;
            s.geometry = pt_cid;
            // Translate to vertex position
            s.tf = Matrix::translate(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z)).to_vec();
            s.add_tag("type", "point");
            components.push_back(s);
        }

        Shape out;
        out.geometry = std::nullopt;
        out.components = components;
        out.add_tag("type", "points");
        vfs->write<Shape>(fulfilling, out);
    }
    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/points"},
            {"description", "Extracts the vertices of the input shape as isolated points."},
            {"arguments", {{"$in", {{"type", "jot:shape"}, {"description", "The shape to extract points from."}}}}},
            {"outputs", {{"$out", {{"type", "shape"}, {"description", "The resulting points shape."}}}}}
        };
    }
};

static void points_init() {
    Processor::register_op<PointsOp<>, Shape>("jot/points");
}

} // namespace geo
} // namespace jotcad
