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
    int _nesting_level;
    FaceInfo2() : in_domain(false), _nesting_level(0) {}
};

template <typename Gt, typename Fb_base = CGAL::Constrained_triangulation_face_base_2<Gt>>
class Face_with_info_2 : public Fb_base {
    FaceInfo2 _info;
public:
    typedef Gt Geom_traits;
    typedef typename Fb_base::Vertex_handle Vertex_handle;
    typedef typename Fb_base::Face_handle   Face_handle;

    template < typename TDS2 >
    struct Rebind_TDS {
        typedef typename Fb_base::template Rebind_TDS<TDS2>::Other Fb2;
        typedef Face_with_info_2<Gt, Fb2> Other;
    };

    Face_with_info_2() : Fb_base() {}
    Face_with_info_2(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2)
        : Fb_base(v0, v1, v2) {}
    Face_with_info_2(Vertex_handle v0, Vertex_handle v1, Vertex_handle v2,
                     Face_handle n0, Face_handle n1, Face_handle n2)
        : Fb_base(v0, v1, v2, n0, n1, n2) {}

    FaceInfo2& info() { return _info; }
    const FaceInfo2& info() const { return _info; }

    bool is_in_domain() const { return _info.in_domain; }
    void set_in_domain(bool b) { _info.in_domain = b; }
    int nesting_level() const { return _info._nesting_level; }
    void set_nesting_level(int l) { _info._nesting_level = l; }
};

typedef CGAL::Exact_predicates_tag Itag;
typedef CGAL::Triangulation_vertex_base_2<EK> Vb;
typedef CGAL::Constrained_triangulation_face_base_2<EK> Fb;
typedef Face_with_info_2<EK, Fb> Fb_with_info;
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
