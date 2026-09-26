#pragma once

#include "types.h"
#include "envelope.h"
#include "beam_search.h"

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Certified result of single-step parting direction optimization.
 */
struct PartingOptimizationResult {
    EK::Vector_3 best_dir;
    ExactMesh solid_wedge;
    ExactMesh solid_piece;
    std::set<size_t> source_faces;
    int cycle_count = 0;
    std::vector<std::vector<Point_3>> boundary_loops_3d;
};

/**
 * @brief Facade that optimizes the next mold parting direction using the Energy-Minimizing Beam Search engine.
 * 
 * Evaluates candidate draw directions using true 3D surface area in mm^2, enforces Handled Purity
 * against the 3D Upper Envelope, and returns the next certified demoldable piece.
 */
inline PartingOptimizationResult optimize_parting_direction(
    const ExactMesh& mesh_part,
    const std::vector<EK::Vector_3>& face_normals,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceBoolMap is_handled,
    const MoldParams& params,
    const std::vector<EK::Vector_3>& prior_draw_dirs = {},
    const std::vector<ExactMesh>& prior_solid_pieces = {}
) {
    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<FT> face_areas(mesh_part.num_faces());
    face_descriptors.reserve(mesh_part.number_of_faces());

    for (auto f : mesh_part.faces()) {
        face_descriptors.push_back(f);
        size_t f_idx = f.idx();
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        face_areas[f_idx] = CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
    }

    size_t unhandled_count = 0;
    for (auto f : face_descriptors) {
        if (!is_handled[f]) unhandled_count++;
    }
    if (unhandled_count == 0) {
        return {};
    }

    // Run priority-driven search to determine the optimal next draw direction and certified piece
    auto decomp = decompose_mold_beam_search(
        mesh_part, face_descriptors, face_normals, face_areas,
        edge_to_faces, is_handled, params,
        /*beam_width=*/3,
        /*max_pieces=*/prior_draw_dirs.size() + 2,
        /*candidates_per_level=*/16,
        prior_draw_dirs,
        prior_solid_pieces
    );

    PartingOptimizationResult res;
    size_t target_idx = prior_draw_dirs.size();
    if (decomp.draw_dirs.size() > target_idx) {
        res.best_dir = decomp.draw_dirs[target_idx];
        if (decomp.solid_wedges.size() > target_idx) {
            res.solid_wedge = decomp.solid_wedges[target_idx];
        }
        if (decomp.solid_pieces.size() > target_idx) {
            res.solid_piece = decomp.solid_pieces[target_idx];
        }
        if (decomp.piece_handled_faces.size() > target_idx) {
            res.source_faces = decomp.piece_handled_faces[target_idx];
        }
        res.cycle_count = 1;
        if (decomp.piece_boundary_loops.size() > target_idx) {
            res.boundary_loops_3d = decomp.piece_boundary_loops[target_idx];
        }
    }
    return res;
}

} // namespace mold
} // namespace geo
} // namespace jotcad
