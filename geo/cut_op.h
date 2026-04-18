#pragma once
#include "impl/protocols.h"
#include "impl/processor.h"
#include "impl/matrix.h"
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>

namespace jotcad {
namespace geo {

typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

template <typename P = JotVfsProtocol>
struct CutOp : P {
    static constexpr const char* path = "jot/cut";

    static void execute(jotcad::fs::VFSNode* vfs, const Shape& in, const std::vector<Shape>& tools, Shape& out) {
        if (tools.empty()) {
            out = in;
            return;
        }

        // 1. Convert Input Shape to GPS (on Z0)
        General_polygon_set_2 target_set;
        shape_to_gps(vfs, in, target_set);

        // 2. Subtract Tools
        for (const auto& tool : tools) {
            General_polygon_set_2 tool_set;
            shape_to_gps(vfs, tool, tool_set);
            target_set.difference(tool_set);
        }

        // 3. Convert GPS back to Geometry
        Geometry result_geo = gps_to_geometry(target_set);
        
        // 4. Wrap and Return
        out = in;
        out.geometry = vfs->write_geometry(result_geo);
        out.add_tag("operation", "cut");
    }

    static void shape_to_gps(jotcad::fs::VFSNode* vfs, const Shape& s, General_polygon_set_2& gps) {
        if (s.geometry.has_value()) {
            // Resolve geometry in its local frame then apply its TF
            Geometry geo = vfs->template read<Geometry>({s.geometry->path, s.geometry->parameters});
            geo.apply_tf(s.tf);

            for (const auto& face : geo.faces) {
                if (face.loops.empty()) continue;

                Polygon_2 boundary;
                for (int idx : face.loops[0]) {
                    boundary.push_back(Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
                }
                if (boundary.is_empty()) continue;
                if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();

                std::vector<Polygon_2> holes;
                for (size_t i = 1; i < face.loops.size(); ++i) {
                    Polygon_2 h;
                    for (int idx : face.loops[i]) {
                        h.push_back(Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
                    }
                    if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                    holes.push_back(h);
                }

                Polygon_with_holes_2 pwh(boundary, holes.begin(), holes.end());
                gps.join(pwh);
            }
        }

        // Recursive components
        for (const auto& child : s.components) {
            // Apply parent's transform to child
            Shape transformed_child = child;
            Matrix parent_m = Matrix::from_vec(s.tf);
            Matrix child_m = Matrix::from_vec(child.tf);
            transformed_child.tf = (parent_m * child_m).to_vec();
            shape_to_gps(vfs, transformed_child, gps);
        }
    }

    static Geometry gps_to_geometry(const General_polygon_set_2& gps) {
        Geometry g;
        std::vector<Polygon_with_holes_2> pwhs;
        gps.polygons_with_holes(std::back_inserter(pwhs));

        for (const auto& pwh : pwhs) {
            Geometry::Face face;
            
            // Outer boundary
            std::vector<int> outer;
            for (auto it = pwh.outer_boundary().vertices_begin(); it != pwh.outer_boundary().vertices_end(); ++it) {
                outer.push_back((int)g.vertices.size());
                g.vertices.push_back({it->x(), it->y(), FT(0)});
            }
            if (!outer.empty()) face.loops.push_back(outer);

            // Holes
            for (auto hit = pwh.holes_begin(); hit != pwh.holes_end(); ++hit) {
                std::vector<int> hole;
                for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                    hole.push_back((int)g.vertices.size());
                    g.vertices.push_back({it->x(), it->y(), FT(0)});
                }
                if (!hole.empty()) face.loops.push_back(hole);
            }
            g.faces.push_back(face);
        }
        return g;
    }

    static std::vector<std::string> argument_keys() { return {"$in", "tools"}; }

    static typename P::json schema() {
        return {
            {"arguments", {
                {"$in", {{"type", "jot:shape"}}},
                {"tools", {{"type", "jot:shapes"}, {"default", nlohmann::json::array()}}}
            }},
            {"inputs", {{"$in", {{"type", "shape"}}}, {"tools", {{"type", "shapes"}}}}},
            {"outputs", {{"$out", {{"type", "shape"}}}}}
        };
    }
};

static void cut_init() {
    Processor::register_op<CutOp<>, Shape, Shape, std::vector<Shape>>();
}

} // namespace geo
} // namespace jotcad
