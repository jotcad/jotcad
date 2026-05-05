#include "triangulation.h"
#include <CGAL/mark_domain_in_triangulation.h>

namespace jotcad {
namespace geo {

void Triangulation::triangulate_face(
    const Geometry::Face& f,
    const std::vector<Vec3>& pts,
    std::function<void(int, int, int)> on_triangle) {
    
    if (f.loops.empty() || f.loops[0].size() < 3) return;

    // 1. Determine local plane from the first loop (robustly)
    int i0 = f.loops[0][0], i1 = f.loops[0][1], i2 = f.loops[0][2];
    EK::Point_3 p0(pts[i0].x, pts[i0].y, pts[i0].z);
    EK::Point_3 p1(pts[i1].x, pts[i1].y, pts[i1].z);
    EK::Point_3 p2(pts[i2].x, pts[i2].y, pts[i2].z);
    EK::Plane_3 plane(p0, p1, p2);

    if (plane.is_degenerate()) {
        // Fallback for nearly collinear start: search for a valid triangle
        bool found = false;
        for (size_t i = 1; i < f.loops[0].size() - 1 && !found; ++i) {
            for (size_t j = i + 1; j < f.loops[0].size(); ++j) {
                p1 = EK::Point_3(pts[f.loops[0][i]].x, pts[f.loops[0][i]].y, pts[f.loops[0][i]].z);
                p2 = EK::Point_3(pts[f.loops[0][j]].x, pts[f.loops[0][j]].y, pts[f.loops[0][j]].z);
                plane = EK::Plane_3(p0, p1, p2);
                if (!plane.is_degenerate()) { found = true; break; }
            }
        }
        if (!found) return;
    }

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
