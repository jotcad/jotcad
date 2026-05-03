#include "triangulation.h"
#include <CGAL/mark_domain_in_triangulation.h>

namespace jotcad {
namespace geo {

void Triangulation::triangulate_face(
    const Geometry::Face& f,
    const std::vector<Vec3>& pts,    std::function<void(int, int, int)> on_triangle) {
    
    if (f.loops.empty()) return;
    
    CDT cdt;
    std::map<CDT::Vertex_handle, int> v_map;
    
    for (const auto& loop : f.loops) {
        std::vector<CDT::Vertex_handle> vh;
        for (int idx : loop) {
            if (idx >= (int)pts.size()) continue;
            vh.push_back(cdt.insert(Point_2(pts[idx].x, pts[idx].y)));
            v_map[vh.back()] = idx;
        }
        if (vh.size() >= 2) {
            for (size_t i = 0; i < vh.size(); ++i) {
                cdt.insert_constraint(vh[i], vh[(i + 1) % vh.size()]);
            }
        }
    }

    // Domain marking via point-in-polygon parity check on centroids across ALL loops
    for (auto fit = cdt.finite_faces_begin(); fit != cdt.finite_faces_end(); ++fit) {
        Point_2 center = CGAL::centroid(fit->vertex(0)->point(), fit->vertex(1)->point(), fit->vertex(2)->point());
        bool inside = false;
        for (const auto& loop : f.loops) {
            for (size_t i = 0, j = loop.size() - 1; i < loop.size(); j = i++) {
                if (((pts[loop[i]].y > center.y()) != (pts[loop[j]].y > center.y())) &&
                    (center.x() < (pts[loop[j]].x - pts[loop[i]].x) * (center.y() - pts[loop[i]].y) / 
                     (pts[loop[j]].y - pts[loop[i]].y) + pts[loop[i]].x)) {
                    inside = !inside;
                }
            }
        }
        if (inside) {
            int i0 = v_map[fit->vertex(0)];
            int i1 = v_map[fit->vertex(1)];
            int i2 = v_map[fit->vertex(2)];
            on_triangle(i0, i1, i2);
        }
    }
}

} // namespace geo
} // namespace jotcad
