#pragma once
#include "protocols.h"
#include "processor.h"
#include "boolean/engine.h"
#include "pack/packaide_core.h"
#include <CGAL/convex_hull_2.h>

namespace jotcad {
namespace geo {

template <typename P = JotVfsProtocol>
struct FootprintOp : P {
    static constexpr const char* path = "jot/footprint";

    static pack::packaide::Polygon_with_holes_2 compute_footprint_polygon(fs::VFSNode* vfs, const Shape& in) {
        std::vector<Shape> leaves;
        for (const auto& node : in.shapes()) {
            if (!node.has_positive_geometry()) continue;
            Geometry geo = vfs->read<Geometry>(node.geometry.value());
            if (!geo.faces.empty()) {
                leaves.push_back(node);
            }
        }
        if (leaves.empty()) {
            for (const auto& node : in.shapes()) {
                if (node.has_positive_geometry()) leaves.push_back(node);
            }
        }
        if (leaves.empty()) return {};

        // Fast path for single 2D planar face/surface
        if (leaves.size() == 1 && leaves[0].geometry.has_value()) {
            Geometry geo = vfs->read<Geometry>(leaves[0].geometry.value());
            if (geo.faces.size() == 1 && !geo.faces[0].loops.empty()) {
                const auto& loops = geo.faces[0].loops;
                pack::packaide::Polygon_2 outer;
                for (int idx : loops[0]) {
                    Point_3 p3 = leaves[0].tf.transform(Point_3(geo.vertices[idx].x, geo.vertices[idx].y, geo.vertices[idx].z));
                    outer.push_back(pack::packaide::Point_2(p3.x(), p3.y()));
                }
                if (outer.is_clockwise_oriented()) outer.reverse_orientation();
                std::vector<pack::packaide::Polygon_2> holes;
                for (size_t l = 1; l < loops.size(); ++l) {
                    pack::packaide::Polygon_2 hole;
                    for (int idx : loops[l]) {
                        Point_3 p3 = leaves[0].tf.transform(Point_3(geo.vertices[idx].x, geo.vertices[idx].y, geo.vertices[idx].z));
                        hole.push_back(pack::packaide::Point_2(p3.x(), p3.y()));
                    }
                    if (hole.is_counterclockwise_oriented()) hole.reverse_orientation();
                    holes.push_back(hole);
                }
                return pack::packaide::Polygon_with_holes_2(outer, holes.begin(), holes.end());
            }
        }

        // 3D Solids / Multi-component convex projection
        std::vector<pack::packaide::Point_2> all_projected_pts;
        for (const auto& leaf : leaves) {
            if (!leaf.geometry.has_value()) continue;
            Geometry geo = vfs->read<Geometry>(leaf.geometry.value());
            for (const auto& v : geo.vertices) {
                Point_3 p3 = leaf.tf.transform(Point_3(v.x, v.y, v.z));
                all_projected_pts.push_back(pack::packaide::Point_2(p3.x(), p3.y()));
            }
        }

        if (all_projected_pts.size() < 3) return {};

        std::vector<pack::packaide::Point_2> hull_pts;
        CGAL::convex_hull_2(all_projected_pts.begin(), all_projected_pts.end(), std::back_inserter(hull_pts));
        if (hull_pts.size() < 3) return {};

        pack::packaide::Polygon_2 p;
        for (const auto& pt : hull_pts) {
            p.push_back(pt);
        }
        if (p.is_clockwise_oriented()) {
            p.reverse_orientation();
        }
        return pack::packaide::Polygon_with_holes_2(p);
    }

    static Shape compute_footprint_shape(fs::VFSNode* vfs, const Shape& in) {
        auto pwh = compute_footprint_polygon(vfs, in);
        if (pwh.outer_boundary().is_empty()) return Shape();

        Geometry out_geo;
        const auto& boundary = pwh.outer_boundary();
        std::vector<int> outer_loop;
        for (auto vit = boundary.vertices_begin(); vit != boundary.vertices_end(); ++vit) {
            int idx = (int)out_geo.vertices.size();
            Vertex v;
            v.x = vit->x();
            v.y = vit->y();
            v.z = FT(0);
            out_geo.vertices.push_back(v);
            outer_loop.push_back(idx);
        }
        
        Geometry::Face out_face;
        out_face.loops.push_back(outer_loop);

        // Add holes if any
        for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
            std::vector<int> hole_loop;
            for (auto vit = hit->vertices_begin(); vit != hit->vertices_end(); ++vit) {
                int idx = (int)out_geo.vertices.size();
                Vertex v;
                v.x = vit->x();
                v.y = vit->y();
                v.z = FT(0);
                out_geo.vertices.push_back(v);
                hole_loop.push_back(idx);
            }
            out_face.loops.push_back(hole_loop);
        }

        out_geo.faces.push_back(out_face);

        boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(out_geo);
        Geometry triangulated = boolean::Engine::mesh_to_geometry(mesh);
        triangulated.faces = out_geo.faces;

        Shape res = P::make_shape(vfs, triangulated, {{"type", "surface"}, {"dim", 2}});
        return res;
    }

    static void execute(fs::VFSNode* vfs, const fs::Selector& fulfilling, const Shape& in) {
        Shape out = in.map_items([&](const Shape& item) {
            return compute_footprint_shape(vfs, item);
        });
        vfs->write(fulfilling.with_output("$out"), out);
    }

    static std::vector<std::string> argument_keys() { return {"$in"}; }
    static typename P::json schema() {
        return {
            {"path", "jot/footprint"},
            {"description", "Projects 3D solid shapes onto the Z=0 ground plane as a 2D polygonal footprint face."},
            {"inputs", {
                {"$in", {{"type", "jot:shape"}, {"description", "The 3D shape or group to project."}}}
            }},
            {"arguments", nlohmann::json::array()},
            {"outputs", {{"$out", {{"type", "jot:shape"}, {"description", "The 2D footprint face."}}}}}
        };
    }
};

static void footprint_init(fs::VFSNode* vfs) {
    Processor::register_op<FootprintOp<>, Shape>(vfs, "jot/footprint");
}

} // namespace geo
} // namespace jotcad
