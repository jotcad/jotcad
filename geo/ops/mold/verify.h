#pragma once
#include "types.h"
#include "boolean/corefine.h"
#include <CGAL/Polygon_mesh_processing/self_intersections.h>

namespace jotcad {
namespace geo {
namespace mold {

inline bool verify_piece_demoldability(
    const MoldPiece& piece,
    const Tree& model_tree,
    const MoldParams& params = MoldParams(),
    int* out_backdraft_count = nullptr,
    FT* out_backdraft_area = nullptr
) {
    if (piece.mesh.is_empty() || piece.mesh.number_of_faces() == 0) return true;
    int backdraft_count = 0;
    FT backdraft_area = FT(0);
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
            FT len_sq = fn.squared_length();
            if (len_sq > FT(0)) {
                bool is_backdraft = false;
                if (min_dot == FT(0)) {
                    if (fn * piece.draw_vector > mold_constants::zero_draft_dot_epsilon()) {
                        is_backdraft = true;
                    }
                } else {
                    FT len = CGAL::approximate_sqrt(len_sq);
                    FT dot = (fn * piece.draw_vector) / len;
                    if (dot > -min_dot) {
                        is_backdraft = true;
                    }
                }
                if (is_backdraft) {
                    backdraft_count++;
                    backdraft_area += CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
                }
            }
        }
    }

    if (out_backdraft_count) {
        *out_backdraft_count = backdraft_count;
    }
    if (out_backdraft_area) {
        *out_backdraft_area = backdraft_area;
    }

    const FT min_backdraft_area_threshold = FT(1) / FT(100); // 0.01 mm^2 threshold for physical undercut
    if (backdraft_area >= min_backdraft_area_threshold) {
        std::cerr << "[Demoldability Warning] Mold piece " << piece.name 
                  << " contains " << backdraft_count << " backdraft faces (area="
                  << CGAL::to_double(backdraft_area) << " mm^2) along draw vector ("
                  << CGAL::to_double(piece.draw_vector.x()) << ", "
                  << CGAL::to_double(piece.draw_vector.y()) << ", "
                  << CGAL::to_double(piece.draw_vector.z()) << ")." << std::endl;

        // Diagnostic Breakdown: classify backdraft faces by model distance & spatial bounds
        FT min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9, min_z_val = 1e9, max_z_val = -1e9;
        EK::Point_3 sum_mid(0, 0, 0);
        int exact_part_faces = 0;
        int boundary_seam_faces = 0;
        EK::Vector_3 sample_fn(0, 0, 0);
        FT max_face_a = 0;

        for (auto f : piece.mesh.faces()) {
            auto h = piece.mesh.halfedge(f);
            auto p0 = piece.mesh.point(piece.mesh.source(h));
            auto p1 = piece.mesh.point(piece.mesh.target(h));
            auto p2 = piece.mesh.point(piece.mesh.target(piece.mesh.next(h)));
            EK::Point_3 mid((p0.x()+p1.x()+p2.x())/FT(3), (p0.y()+p1.y()+p2.y())/FT(3), (p0.z()+p1.z()+p2.z())/FT(3));
            FT dist_sq = model_tree.squared_distance(mid);
            if (dist_sq < FT(1) / FT(10000)) {
                EK::Vector_3 fn = CGAL::normal(p0, p1, p2);
                if (fn.squared_length() > FT(0)) {
                    bool is_bd = (min_dot == FT(0)) ? (fn * piece.draw_vector > mold_constants::zero_draft_dot_epsilon())
                                                    : ((fn * piece.draw_vector) / CGAL::approximate_sqrt(fn.squared_length()) > -min_dot);
                    if (is_bd) {
                        if (mid.x() < min_x) min_x = mid.x();
                        if (mid.x() > max_x) max_x = mid.x();
                        if (mid.y() < min_y) min_y = mid.y();
                        if (mid.y() > max_y) max_y = mid.y();
                        if (mid.z() < min_z_val) min_z_val = mid.z();
                        if (mid.z() > max_z_val) max_z_val = mid.z();
                        sum_mid = EK::Point_3(sum_mid.x() + mid.x(), sum_mid.y() + mid.y(), sum_mid.z() + mid.z());
                        if (dist_sq < FT(1) / FT(1000000)) {
                            exact_part_faces++;
                        } else {
                            boundary_seam_faces++;
                        }
                        FT fa = CGAL::squared_area(p0, p1, p2);
                        if (fa > max_face_a) {
                            max_face_a = fa;
                            sample_fn = fn;
                        }
                    }
                }
            }
        }
        if (backdraft_count > 0) {
            EK::Point_3 centroid(sum_mid.x() / FT(backdraft_count), sum_mid.y() / FT(backdraft_count), sum_mid.z() / FT(backdraft_count));
            std::cerr << "      ↳ [Backdraft Diagnostic] Centroid: (" << CGAL::to_double(centroid.x()) << ", "
                      << CGAL::to_double(centroid.y()) << ", " << CGAL::to_double(centroid.z()) << ") | "
                      << "Classification: " << exact_part_faces << " exact part faces (dist<1e-3), "
                      << boundary_seam_faces << " seam/boundary faces (1e-3<=dist<0.01) | "
                      << "BB: [" << CGAL::to_double(min_x) << ".." << CGAL::to_double(max_x) << "] x ["
                      << CGAL::to_double(min_y) << ".." << CGAL::to_double(max_y) << "] x ["
                      << CGAL::to_double(min_z_val) << ".." << CGAL::to_double(max_z_val) << "] | "
                      << "Sample normal: (" << CGAL::to_double(sample_fn.x()) << ", " << CGAL::to_double(sample_fn.y())
                      << ", " << CGAL::to_double(sample_fn.z()) << ") dot=" << CGAL::to_double(sample_fn * piece.draw_vector)
                      << std::endl;
        }
        return false;
    } else if (backdraft_count > 0) {
        std::cerr << "[Demoldability] Ignored microscopic boundary noise: " 
                  << backdraft_count << " sliver faces (area=" 
                  << CGAL::to_double(backdraft_area) << " mm^2 < 0.01 mm^2)." << std::endl;
    }
    return true;
}

/**
 * @brief Strictly validates a tentative candidate piece prior to child derivation or selection.
 * 
 * 1. Cuts solid wedge against model (CSG difference: wedge \ model).
 * 2. Subtracts all previously validated pieces in the chain.
 * 3. Enforces 2-manifold closed watertight topology.
 * 4. Verifies 0 backdraft faces via verify_piece_demoldability.
 * 
 * Returns true if validated (storing cut mesh in out_piece_mesh), false if eliminated.
 */
inline bool validate_tentative_candidate(
    const ExactMesh& model_mesh,
    const Tree& model_tree,
    const ExactMesh& solid_wedge,
    const EK::Vector_3& draw_dir,
    const std::vector<ExactMesh>& prior_solid_pieces,
    const MoldParams& params,
    ExactMesh& out_piece_mesh,
    int* out_backdraft_count = nullptr
) {
    if (solid_wedge.number_of_faces() == 0) return false;
    if (!params.molds) {
        return true;
    }

    // 1. Cut wedge against model: piece_mesh = solid_wedge \ model_mesh
    ExactMesh piece_mesh;
    ExactMesh wedge_copy = solid_wedge;
    ExactMesh model_copy = model_mesh;
    bool ok_diff = boolean::corefine_difference(
        wedge_copy, model_copy, piece_mesh,
        params.kiss_mode, params.kiss_width,
        "tentative_wedge \\ model in validation"
    );
    if (!ok_diff || piece_mesh.is_empty() || piece_mesh.number_of_faces() == 0) {
        return false;
    }

    // 2. Subtract all previously validated solid pieces: piece_mesh \ prior_pieces
    for (const auto& prev_piece : prior_solid_pieces) {
        if (prev_piece.is_empty() || prev_piece.number_of_faces() == 0) continue;
        if (!boolean::do_meshes_overlap(piece_mesh, prev_piece)) continue;

        ExactMesh non_overlapping;
        bool ok_pdiff = boolean::corefine_difference(
            piece_mesh, prev_piece, non_overlapping,
            params.kiss_mode, params.kiss_width,
            "tentative_piece \\ prev_piece in validation"
        );
        if (!ok_pdiff || non_overlapping.is_empty() || non_overlapping.number_of_faces() == 0) {
            return false;
        }
        piece_mesh = std::move(non_overlapping);
        piece_mesh.collect_garbage();
    }

    // 3. Topology check: must be a closed watertight 2-manifold
    if (!CGAL::is_closed(piece_mesh)) {
        return false;
    }

    // 4. Authoritative swept-volume demoldability check (0 backdraft faces)
    MoldPiece cand_piece{piece_mesh, draw_dir, "tentative_piece", "#2bee2b", (int)prior_solid_pieces.size() + 1};
    int backdraft_count = 0;
    bool is_demoldable = verify_piece_demoldability(cand_piece, model_tree, params, &backdraft_count);
    if (out_backdraft_count) {
        *out_backdraft_count = backdraft_count;
    }

    if (!is_demoldable || backdraft_count > 0) {
        return false;
    }

    out_piece_mesh = std::move(piece_mesh);
    return true;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
