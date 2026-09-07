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
        pack::packaide::Polygon_set_2 pset;
        std::vector<pack::packaide::Point_2> solid_points;

        for (const auto& node : in.shapes()) {
            if (!node.has_positive_geometry()) continue;
            Geometry geo = vfs->read<Geometry>(node.geometry.value());
            boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
            if (mesh.is_empty()) continue;
            boolean::Engine::transform_mesh(mesh, node.tf);

            // Check if this component has border halfedges (open surface / 2D face) or is a closed solid
            bool has_borders = false;
            for (auto h : mesh.halfedges()) {
                if (mesh.is_border(h)) {
                    has_borders = true;
                    break;
                }
            }

            if (has_borders) {
                // 2D Surface / Open Mesh: join each face into the polygon set
                for (auto f : mesh.faces()) {
                    pack::packaide::Polygon_2 f_poly;
                    for (auto v : mesh.vertices_around_face(mesh.halfedge(f))) {
                        auto p3 = mesh.point(v);
                        f_poly.push_back(pack::packaide::Point_2(p3.x(), p3.y()));
                    }
                    if (f_poly.size() >= 3) {
                        if (f_poly.is_clockwise_oriented()) f_poly.reverse_orientation();
                        if (f_poly.is_simple()) {
                            pset.join(f_poly);
                        }
                    }
                }
            } else {
                // Closed 3D Solid: collect points for 2D ground projection
                for (auto v : mesh.vertices()) {
                    auto p3 = mesh.point(v);
                    solid_points.push_back(pack::packaide::Point_2(p3.x(), p3.y()));
                }
            }
        }

        if (!solid_points.empty()) {
            std::vector<pack::packaide::Point_2> hull;
            CGAL::convex_hull_2(solid_points.begin(), solid_points.end(), std::back_inserter(hull));
            if (hull.size() >= 3) {
                pack::packaide::Polygon_2 p;
                for (const auto& pt : hull) p.push_back(pt);
                if (p.is_clockwise_oriented()) p.reverse_orientation();
                pset.join(p);
            }
        }

        std::vector<pack::packaide::Polygon_with_holes_2> pwhs;
        pset.polygons_with_holes(std::back_inserter(pwhs));
        if (pwhs.empty()) return {};

        // Return the largest polygon by outer boundary area
        size_t best_idx = 0;
        pack::packaide::FT best_area = pwhs[0].outer_boundary().area();
        for (size_t i = 1; i < pwhs.size(); ++i) {
            auto a = pwhs[i].outer_boundary().area();
            if (a > best_area) {
                best_area = a;
                best_idx = i;
            }
        }
        return pwhs[best_idx];
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

        Shape res = P::make_shape(vfs, out_geo, {{"type", "surface"}});
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
