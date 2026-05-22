#include "triangulation.h"
#include <CGAL/mark_domain_in_triangulation.h>
#include <CGAL/normal_vector_newell_3.h>

namespace jotcad {
namespace geo {

void Triangulation::triangulate_face(
    const Geometry::Face& f,
    const std::vector<Vec3>& pts,
    std::function<void(int, int, int)> on_triangle) {
    
    if (f.loops.empty() || f.loops[0].size() < 3) return;

    // 1. Determine local plane using Newell's Method (robust for non-convex polygons)
    std::vector<EK::Point_3> loop_pts;
    loop_pts.reserve(f.loops[0].size());
    for (int idx : f.loops[0]) {
        loop_pts.emplace_back(pts[idx].x, pts[idx].y, pts[idx].z);
    }
    
    EK::Vector_3 normal;
    CGAL::normal_vector_newell_3(loop_pts.begin(), loop_pts.end(), normal);

    if (normal == CGAL::NULL_VECTOR) return; // Degenerate face
    EK::Plane_3 plane(loop_pts[0], normal);

    // 2. Project 3D points to local 2D space for stable CDT
    CDT cdt;
    std::map<CDT::Vertex_handle, int> v_map;
    for (const auto& loop : f.loops) {
        std::vector<CDT::Vertex_handle> vh;
        for (int idx : loop) {
            EK::Point_3 p_3d(pts[idx].x, pts[idx].y, pts[idx].z);
            vh.push_back(cdt.insert(plane.to_2d(p_3d)));
            v_map[vh.back()] = idx;
        }
        if (vh.size() >= 2) {
            for (size_t i = 0; i < vh.size(); ++i) {
                cdt.insert_constraint(vh[i], vh[(i + 1) % vh.size()]);
            }
        }
    }

    // 3. Mark domain and output triangles
    CGAL::mark_domain_in_triangulation(cdt);
    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        if (fit->info().in_domain) {
            on_triangle(v_map[fit->vertex(0)], v_map[fit->vertex(1)], v_map[fit->vertex(2)]);
        }
    }
}

} // namespace geo
} // namespace jotcad
