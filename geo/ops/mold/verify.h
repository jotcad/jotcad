#pragma once
#include "types.h"

namespace jotcad {
namespace geo {
namespace mold {

inline void verify_piece_demoldability(
    const MoldPiece& piece,
    const Tree& model_tree,
    const MoldParams& params = MoldParams()
) {
    if (piece.mesh.is_empty() || piece.mesh.number_of_faces() == 0) return;
    int backdraft_count = 0;
    FT min_dot(std::sin(CGAL::to_double(params.draft) * 2.0 * M_PI));
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
            // Opening clearance requires (-fn / |fn|) * d >= min_dot <=> (fn / |fn|) * d <= -min_dot
            double len = std::sqrt(CGAL::to_double(fn.squared_length()));
            if (len > 1e-9) {
                FT dot = (fn * piece.draw_vector) / FT(len);
                if (dot > -min_dot) {
                    backdraft_count++;
                }
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
