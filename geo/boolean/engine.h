#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits.h>
#include <CGAL/AABB_face_graph_triangle_primitive.h>
#include <CGAL/intersections.h>
#include <vector>
#include <algorithm>
#include <variant>
#include "../impl/kernel.h"
#include "../fix/repair.h"

namespace jotcad {
namespace geo {
namespace boolean {

typedef CGAL::Surface_mesh<EK::Point_3> Surface_mesh;
typedef CGAL::Gps_segment_traits_2<EK> Gps_traits_2;
typedef CGAL::General_polygon_set_2<Gps_traits_2> General_polygon_set_2;
typedef CGAL::Polygon_2<EK> Polygon_2;
typedef CGAL::Polygon_with_holes_2<EK> Polygon_with_holes_2;

struct Engine {
    /**
     * cut_mesh_by_mesh: 3D Volume-Volume subtraction.
     */
    static bool cut_mesh_by_mesh(Surface_mesh& target, Surface_mesh& tool) {
        if (!CGAL::is_closed(target) || !CGAL::is_closed(tool)) {
            return CGAL::Polygon_mesh_processing::clip(
                target, tool, 
                CGAL::parameters::use_compact_clipper(true).clip_volume(false)
            );
        }

        bool success = CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
            target, tool, target,
            CGAL::parameters::throw_on_self_intersection(false)
        );

        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * cut_mesh_by_plane: 3D Mesh split by infinite plane.
     */
    static bool cut_mesh_by_plane(Surface_mesh& target, const EK::Plane_3& plane) {
        bool success = CGAL::Polygon_mesh_processing::clip(
            target, plane,
            CGAL::parameters::use_compact_clipper(true).clip_volume(true)
        );
        fix::make_geometry_unambiguous(target);
        return success;
    }

    /**
     * cut_points_by_mesh: 0D Point removal by 3D Volume.
     */
    static void cut_points_by_mesh(std::vector<EK::Point_3>& points, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) return; 
        
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);
        points.erase(
            std::remove_if(points.begin(), points.end(),
                [&](const EK::Point_3& p) {
                    return inside_check(p) == CGAL::ON_BOUNDED_SIDE;
                }),
            points.end()
        );
    }

    /**
     * cut_segments_by_mesh: 1D Segment clipping by 3D Volume.
     * Keeps portions of segments that are OUTSIDE the tool.
     */
    static void cut_segments_by_mesh(std::vector<std::pair<EK::Point_3, EK::Point_3>>& segments, const Surface_mesh& tool) {
        if (!CGAL::is_closed(tool)) return;

        typedef CGAL::AABB_face_graph_triangle_primitive<Surface_mesh> Primitive;
        typedef CGAL::AABB_traits<EK, Primitive> Traits;
        typedef CGAL::AABB_tree<Traits> Tree;
        
        Tree tree(faces(tool).first, faces(tool).second, tool);
        CGAL::Side_of_triangle_mesh<Surface_mesh, EK> inside_check(tool);

        std::vector<std::pair<EK::Point_3, EK::Point_3>> result;

        for (const auto& seg : segments) {
            EK::Segment_3 s(seg.first, seg.second);
            std::vector<Tree::Intersection_and_primitive_id<EK::Segment_3>::Type> intersections;
            tree.all_intersections(s, std::back_inserter(intersections));

            if (intersections.empty()) {
                if (inside_check(seg.first) != CGAL::ON_BOUNDED_SIDE) {
                    result.push_back(seg);
                }
                continue;
            }

            std::vector<EK::Point_3> split_pts;
            split_pts.push_back(seg.first);
            for (auto const& inter : intersections) {
                // Access using std::get_if for std::variant results
                if (const EK::Point_3* p = std::get_if<EK::Point_3>(&inter.first)) {
                    split_pts.push_back(*p);
                } else if (const EK::Segment_3* s_inter = std::get_if<EK::Segment_3>(&inter.first)) {
                    split_pts.push_back(s_inter->source());
                    split_pts.push_back(s_inter->target());
                }
            }
            split_pts.push_back(seg.second);

            EK::Vector_3 dir = seg.second - seg.first;
            std::sort(split_pts.begin(), split_pts.end(), [&](const EK::Point_3& a, const EK::Point_3& b) {
                return (a - seg.first) * dir < (b - seg.first) * dir;
            });

            for (size_t i = 0; i < split_pts.size() - 1; ++i) {
                if (split_pts[i] == split_pts[i+1]) continue;
                EK::Point_3 mid = CGAL::midpoint(split_pts[i], split_pts[i+1]);
                if (inside_check(mid) != CGAL::ON_BOUNDED_SIDE) {
                    result.push_back({split_pts[i], split_pts[i+1]});
                }
            }
        }
        segments = std::move(result);
    }

    /**
     * cut_gps_by_gps: 2D Polygon-Polygon subtraction.
     */
    static void cut_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) {
        target.difference(tool);
    }
};

} // namespace boolean
} // namespace geo
} // namespace jotcad
