#pragma once
#include "geometry.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/minkowski_sum_2.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <vector>
#include <cmath>
#include <cassert>

namespace jotcad {
namespace geo {

typedef CGAL::Exact_predicates_exact_constructions_kernel EK;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;
typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;

static void applyOffset(Geometry& geo, FT k) {
    if (k == 0) return;
    
    // 1. Prepare circular tool approximation
    Polygon_2 tool;
    int tool_segments = 16;
    double d_abs_k = CGAL::to_double(CGAL::abs(k));
    for (int i = 0; i < tool_segments; ++i) {
        double a = 2.0 * M_PI * i / tool_segments;
        tool.push_back(Point_2(d_abs_k * std::cos(a), d_abs_k * std::sin(a)));
    }
    if (tool.is_clockwise_oriented()) tool.reverse_orientation();
    assert(tool.is_simple() && "Offset tool must be simple");

    Geometry result_geo;

    for (auto& face : geo.faces) {
        if (face.loops.empty()) continue;
        
        // 2. Build Subject Loops
        Polygon_2 boundary;
        for (int idx : face.loops[0]) {
            boundary.push_back(Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
        }
        if (boundary.size() < 3 || !boundary.is_simple()) continue; 
        if (boundary.is_clockwise_oriented()) boundary.reverse_orientation();

        std::vector<Polygon_2> holes;
        for (size_t i = 1; i < face.loops.size(); ++i) {
            Polygon_2 h;
            for (int idx : face.loops[i]) h.push_back(Point_2(geo.vertices[idx].x, geo.vertices[idx].y));
            if (h.size() >= 3 && h.is_simple()) {
                if (h.is_counterclockwise_oriented()) h.reverse_orientation();
                holes.push_back(h);
            }
        }

        // 3. Perform Offset
        std::vector<Polygon_with_holes_2> results;
        if (k > 0) {
            // Expansion: Minkowski sum handles holes correctly
            Polygon_with_holes_2 pwh(boundary, holes.begin(), holes.end());
            results.push_back(CGAL::minkowski_sum_2(pwh, tool));
        } else {
            // Contraction: Use Framed Hole method for stability
            CGAL::Bbox_2 bb = boundary.bbox();
            double margin = d_abs_k * 4.0;
            Polygon_2 frame;
            frame.push_back(Point_2(bb.xmin() - margin, bb.ymin() - margin));
            frame.push_back(Point_2(bb.xmax() + margin, bb.ymin() - margin));
            frame.push_back(Point_2(bb.xmax() + margin, bb.ymax() + margin));
            frame.push_back(Point_2(bb.xmin() - margin, bb.ymax() + margin));
            if (frame.is_clockwise_oriented()) frame.reverse_orientation();
            assert(frame.is_simple() && "Inward frame must be simple");

            // Subject's boundary becomes a hole in the frame
            Polygon_2 inv_boundary = boundary;
            inv_boundary.reverse_orientation();
            Polygon_with_holes_2 framed_pwh(frame);
            framed_pwh.add_hole(inv_boundary);

            // Expand frame inwards
            Polygon_with_holes_2 expanded_frame = CGAL::minkowski_sum_2(framed_pwh, tool);

            // Resulting holes are the inset boundaries
            General_polygon_set_2 gps;
            for (auto hit = expanded_frame.holes_begin(); hit != expanded_frame.holes_end(); ++hit) {
                Polygon_2 b = *hit;
                assert(b.is_simple() && "Inset boundary must be simple");
                b.reverse_orientation(); // CCW
                gps.join(General_polygon_set_2(b));
            }

            // Subtract expanded holes
            for (const auto& h : holes) {
                Polygon_with_holes_2 expanded_hole = CGAL::minkowski_sum_2(h, tool);
                gps.difference(General_polygon_set_2(expanded_hole));
            }
            
            if (!gps.is_empty()) {
                gps.polygons_with_holes(std::back_inserter(results));
            }
        }

        // 4. Sink Result
        for (const auto& res : results) {
            Geometry::Face new_face;
            std::vector<int> outer;
            for (auto it = res.outer_boundary().vertices_begin(); it != res.outer_boundary().vertices_end(); ++it) {
                outer.push_back((int)result_geo.vertices.size());
                result_geo.vertices.push_back({it->x(), it->y(), FT(0)});
            }
            if (!outer.empty()) new_face.loops.push_back(outer);
            for (auto hit = res.holes_begin(); hit != res.holes_end(); ++hit) {
                std::vector<int> hole;
                for (auto it = hit->vertices_begin(); it != hit->vertices_end(); ++it) {
                    hole.push_back((int)result_geo.vertices.size());
                    result_geo.vertices.push_back({it->x(), it->y(), FT(0)});
                }
                if (!hole.empty()) new_face.loops.push_back(hole);
            }
            if (!new_face.loops.empty()) result_geo.faces.push_back(new_face);
        }
    }
    
    if (!result_geo.faces.empty()) geo = result_geo;
    else { geo.faces.clear(); geo.vertices.clear(); }
}

} // namespace geo
} // namespace jotcad
