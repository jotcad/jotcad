#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/clip.h>
#include <CGAL/Polygon_mesh_processing/repair.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/General_polygon_set_2.h>
#include <CGAL/Gps_segment_traits_2.h>
#include <CGAL/Side_of_triangle_mesh.h>
#include <vector>
#include <algorithm>
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
            CGAL::parameters::use_compact_clipper(true)
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
     * cut_gps_by_gps: 2D Polygon-Polygon subtraction.
     */
    static void cut_gps_by_gps(General_polygon_set_2& target, const General_polygon_set_2& tool) {
        target.difference(tool);
    }
};

} // namespace boolean
} // namespace geo
} // namespace jotcad
