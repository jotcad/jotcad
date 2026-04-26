#pragma once
#include <vector>
#include <map>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include "geometry.h"
#include "camera.h"

namespace jotcad {
namespace geo {

struct FaceInfo2 {
    bool in_domain;
    FaceInfo2() : in_domain(false) {}
};

typedef CGAL::Exact_predicates_tag Itag;
typedef CGAL::Triangulation_vertex_base_2<EK> Vb;
typedef CGAL::Constrained_triangulation_face_base_2<EK> Fb;
typedef CGAL::Triangulation_face_base_with_info_2<FaceInfo2, EK, Fb> Fb_with_info;
typedef CGAL::Triangulation_data_structure_2<Vb, Fb_with_info> TDS;
typedef CGAL::Constrained_Delaunay_triangulation_2<EK, TDS, Itag> CDT;

struct Triangulation {
    /**
     * triangulate_face: Robustly decomposes a complex face into triangles using CDT.
     */
    static void triangulate_face(
        const Geometry::Face& f, 
        const std::vector<Vec3>& projected_pts,
        std::function<void(int, int, int)> on_triangle);
};

} // namespace geo
} // namespace jotcad
