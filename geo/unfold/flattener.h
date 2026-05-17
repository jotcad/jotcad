#pragma once

#include <CGAL/Surface_mesh.h>
#include <CGAL/boost/graph/helpers.h>
#include "../boolean/engine.h"

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
        
        // Exact coplanarity check: planes are equal or opposite (back-to-back)
        return p1 == p2 || p1 == p2.opposite();
    }
};

} // namespace unfold
} // namespace geo
} // namespace jotcad
