#pragma once
#include "types.h"

namespace jotcad {
namespace geo {
namespace mold {

inline void verify_piece_demoldability(
    const MoldPiece& piece,
    const Tree& model_tree
) {
    int backdraft_count = 0;
    for (auto f : piece.mesh.faces()) {
        auto h = piece.mesh.halfedge(f);
        auto p0 = piece.mesh.point(piece.mesh.source(h));
        auto p1 = piece.mesh.point(piece.mesh.target(h));
        auto p2 = piece.mesh.point(piece.mesh.target(piece.mesh.next(h)));
        EK::Point_3 mid((p0.x()+p1.x()+p2.x())/FT(3), (p0.y()+p1.y()+p2.y())/FT(3), (p0.z()+p1.z()+p2.z())/FT(3));
        
        // Check if this face touches the model (cavity face)
        if (model_tree.squared_distance(mid) < FT(1) / FT(10000)) {
            EK::Vector_3 fn = CGAL::normal(p0, p1, p2);
            // On cavity faces, mold normal points inward toward model (-model_normal).
            // Opening clearance requires mold withdrawal vector d dot mold_normal >= 0
            if (fn * piece.draw_vector < FT(0)) {
                backdraft_count++;
            }
        }
    }

    if (backdraft_count > 0) {
        std::cerr << "[Warning] Mold piece " << piece.name << " contains " << backdraft_count << " backdraft faces." << std::endl;
    }
}

} // namespace mold
} // namespace geo
} // namespace jotcad
