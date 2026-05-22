#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/helpers.h>
#include "../boolean/engine.h"
#include "../math/matrix.h"
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

namespace jotcad {
namespace geo {
namespace unfold {

class Flattener {
public:
    typedef boolean::Surface_mesh Mesh;
    typedef Mesh::Face_index Face;
    typedef Mesh::Edge_index Edge;
    typedef Mesh::Halfedge_index Halfedge;

    /**
     * get_face_plane: Returns the 3D plane of a face.
     */
    static EK::Plane_3 get_face_plane(const Mesh& mesh, Face f) {
        auto h = mesh.halfedge(f);
        return EK::Plane_3(
            mesh.point(mesh.source(h)),
            mesh.point(mesh.target(h)),
            mesh.point(mesh.target(mesh.next(h)))
        );
    }

    /**
     * is_coplanar: Checks if two faces lie on the same 3D plane.
     */
    static bool is_coplanar(const Mesh& mesh, Face f1, Face f2) {
        EK::Plane_3 p1 = get_face_plane(mesh, f1);
        EK::Plane_3 p2 = get_face_plane(mesh, f2);
        return p1 == p2 || p1 == p2.opposite();
    }

    /**
     * get_face_transform: Returns a 3D-to-2D transform that flattens a face onto the XY plane.
     */
    static Matrix get_face_transform(const Mesh& mesh, Face f) {
        auto h = mesh.halfedge(f);
        EK::Point_3 origin = mesh.point(mesh.source(h));
        EK::Vector_3 normal = CGAL::Polygon_mesh_processing::compute_face_normal(f, mesh);
        return Matrix::lookAt(origin, normal);
    }

    /**
     * transform_gps: Manually transforms a General_polygon_set_2 since it lacks a .transform() member.
     */
    static void transform_gps(boolean::General_polygon_set_2& gps, const Matrix& m) {
        std::vector<boolean::Polygon_with_holes_2> pwhs;
        gps.polygons_with_holes(std::back_inserter(pwhs));
        gps.clear();
        auto tf = m.to_cgal_2d();
        for (auto& pwh : pwhs) {
            gps.join(CGAL::transform(tf, pwh));
        }
    }

    /**
     * get_2d_snap_transform: Calculates the exact rigid 2D snap transform.
     */
    static Matrix get_2d_snap_transform(const EK::Point_2& a1, const EK::Point_2& a2,
                                       const EK::Point_2& b1, const EK::Point_2& b2) {
        EK::Vector_2 vA = a2 - a1;
        EK::Vector_2 vB = b2 - b1;
        EK::FT L2 = vB.x()*vB.x() + vB.y()*vB.y();
        
        EK::FT cos_t = (vA.x()*vB.x() + vA.y()*vB.y()) / L2;
        EK::FT sin_t = (vA.y()*vB.x() - vA.x()*vB.y()) / L2;

        EK::FT tx = a1.x() - cos_t * b1.x() + sin_t * b1.y();
        EK::FT ty = a1.y() - sin_t * b1.x() - cos_t * b1.y();

        Transformation trans(
            cos_t, -sin_t, 0, tx,
            sin_t,  cos_t, 0, ty,
            0,      0,     1, 0
        );
        return Matrix(trans);
    }
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
