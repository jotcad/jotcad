#pragma once

#include "types.h"
#include "scoring.h"
#include "candidates.h"
#include "patch.h"
#include "compatibility.h"
#include "envelope.h"
#include "verify.h"
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {
namespace mold {

/**
 * @brief Node representing an active candidate chain in the search frontier.
 */
struct MoldChainNode {
    std::vector<EK::Vector_3> draw_dirs;
    std::vector<ExactMeshPtr> solid_wedges;
    std::vector<ExactMeshPtr> solid_pieces; // Realized and certified validated solid pieces
    ExactMeshPtr remaining_stock;           // Active residual stock: B_k = B_{k-1} \ P_k
    std::vector<std::set<size_t>> piece_handled_faces;
    std::vector<std::vector<std::vector<Point_3>>> piece_boundary_loops;
    std::vector<bool> is_handled;
    size_t handled_count = 0;
    FT unhandled_area = FT(0);
    FT energy = FT(0);
    FT total_score = FT(0);
    CandidateStatus status = CandidateStatus::TENTATIVE;
};

/**
 * @brief Certified result of multi-piece mold decomposition.
 */
struct MoldDecompositionResult {
    std::vector<EK::Vector_3> draw_dirs;
    std::vector<ExactMesh> solid_pieces; // Certified validated solid pieces
    std::vector<ExactMesh> solid_wedges;
    std::vector<std::set<size_t>> piece_handled_faces;
    std::vector<std::vector<std::vector<Point_3>>> piece_boundary_loops;
    bool is_complete = false;
    FT final_energy = FT(0);
    FT remaining_unhandled_area = FT(0);
};

/**
 * @brief Orchestrates multi-piece mold decomposition as a Priority-Driven Search with Tentative & Validated Candidates.
 * 
 * 1. Generates large pools of Tentative Candidates, fully scored up front with encroachment penalties.
 * 2. Carves each candidate directly from active residual stock: P_k = B_{k-1} ∩ W_k.
 * 3. Enforces that only VALIDATED candidates may derive child candidates or be selected as final solutions.
 * 4. At terminal piece K, assigns remaining residual stock by complementation: P_K = B_{K-1}.
 */
inline MoldDecompositionResult decompose_mold_beam_search(
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    FaceBoolMap is_handled_map,
    const MoldParams& params,
    size_t max_pieces = 6,
    size_t candidates_per_level = 16,
    const std::vector<EK::Vector_3>& prior_draw_dirs = {},
    const std::vector<ExactMesh>& prior_solid_pieces = {},
    ExactMeshPtr initial_stock = nullptr,
    const ExactMesh* stock_box_mesh = nullptr
) {
    FT min_dot(std::sin(CGAL::to_double(params.draft) * 2.0 * M_PI));
    FT total_area = FT(0);
    for (auto a : face_areas) total_area += a;

    Tree model_tree(CGAL::faces(mesh_part).first, CGAL::faces(mesh_part).second, mesh_part);
    model_tree.build();

    // Root node: Depth 0 / prior pieces state (VALIDATED by invariant)
    MoldChainNode root;
    root.draw_dirs = prior_draw_dirs;
    for (const auto& p : prior_solid_pieces) {
        root.solid_pieces.push_back(std::make_shared<const ExactMesh>(p));
    }
    root.status = CandidateStatus::VALIDATED;
    root.is_handled.resize(mesh_part.num_faces());
    for (auto f : face_descriptors) {
        bool handled = is_handled_map[f];
        root.is_handled[f.idx()] = handled;
        if (handled) {
            root.handled_count++;
        } else {
            root.unhandled_area += face_areas[f.idx()];
        }
    }
    const FT lambda_pieces(50); // Regularizer: 50 mm^2 penalty per piece
    root.energy = root.unhandled_area + lambda_pieces * FT(root.draw_dirs.size());

    // Initialize root residual stock B_0 = B \ M
    ExactMeshPtr active_stock = initial_stock;
    if (!active_stock) {
        FT xmin = 1e9, xmax = -1e9, ymin = 1e9, ymax = -1e9, zmin = 1e9, zmax = -1e9;
        for (auto v : mesh_part.vertices()) {
            auto p = mesh_part.point(v);
            if (p.x() < xmin) xmin = p.x();
            if (p.x() > xmax) xmax = p.x();
            if (p.y() < ymin) ymin = p.y();
            if (p.y() > ymax) ymax = p.y();
            if (p.z() < zmin) zmin = p.z();
            if (p.z() > zmax) zmax = p.z();
        }
        ExactMesh box_m = boolean::Engine::geometry_to_mesh(build_box_geo(
            xmin - params.padding, xmax + params.padding,
            ymin - params.padding, ymax + params.padding,
            zmin - params.padding, zmax + params.padding
        ));
        ExactMesh s0;
        boolean::corefine_difference(box_m, mesh_part, s0, params.kiss_mode, params.kiss_width, "default stock \\ model in beam search");
        active_stock = std::make_shared<const ExactMesh>(std::move(s0));
    }
    root.remaining_stock = active_stock;

    // If initial state is already 100% complete, return certified complete
    if (root.handled_count == face_descriptors.size() || root.unhandled_area <= FT(0)) {
        return {prior_draw_dirs, prior_solid_pieces, {}, {}, {}, true, root.energy, FT(0)};
    }

    // Global Priority Frontier across all search depths (stores exclusively VALIDATED candidates)
    std::vector<MoldChainNode> frontier = { root };
    const size_t max_frontier_size = 50;
    size_t validations_count = 0;

    MoldChainNode best_complete;
    bool found_complete = false;
    MoldChainNode best_partial = root;

    std::cout << "    [BeamSearch] Starting Level-by-Level Priority Search (frontier_bound=" << max_frontier_size 
              << ", max_pieces=" << max_pieces << ", prior_pieces=" << prior_draw_dirs.size()
              << ", unhandled_area=" << CGAL::to_double(root.unhandled_area) 
              << " mm^2)..." << std::endl;

    while (!frontier.empty()) {
        // Pop lowest-energy validated candidate node from frontier
        MoldChainNode curr = std::move(frontier.front());
        frontier.erase(frontier.begin());

        // Track best validated partial solution
        if (curr.handled_count > best_partial.handled_count || 
           (curr.handled_count == best_partial.handled_count && curr.energy < best_partial.energy)) {
            best_partial = curr;
        }

        // Selection Gate: Certified complete decomposition?
        if (curr.unhandled_area <= FT(0) || curr.handled_count == face_descriptors.size()) {
            std::cout << "    [BeamSearch] Certified complete decomposition found with "
                      << curr.draw_dirs.size() << " pieces! (Energy=" << CGAL::to_double(curr.energy) << ")" << std::endl;
            best_complete = std::move(curr);
            found_complete = true;
            break;
        }

        // If max pieces reached for this chain, cannot derive further
        if (curr.draw_dirs.size() >= max_pieces) {
            continue;
        }

        // Derivation Gate: Converge Level k before progressing using the best candidate
        for (auto f : face_descriptors) {
            is_handled_map[f] = curr.is_handled[f.idx()];
        }

        auto candidate_dirs = generate_candidate_directions(
            mesh_part, face_descriptors, face_normals, face_areas,
            is_handled_map, curr.draw_dirs, /*num_exploratory=*/32
        );

        struct ScoredCandidate {
            EK::Vector_3 dir;
            CandidateScore score;
        };
        std::vector<ScoredCandidate> scored_cands;
        for (const auto& d : candidate_dirs) {
            auto score = score_candidate_direction(
                d, face_descriptors, face_normals, face_areas, is_handled_map, min_dot
            );
            if (score.responsible_area > FT(0)) {
                scored_cands.push_back({d, score});
            }
        }

        std::sort(scored_cands.begin(), scored_cands.end(), [](const auto& a, const auto& b) {
            return a.score > b.score;
        });

        std::vector<MoldChainNode> level_validated_children;
        size_t candidate_attempts = 0;
        const size_t max_attempts_per_level = candidates_per_level * 2;

        for (const auto& sc : scored_cands) {
            if (candidate_attempts >= max_attempts_per_level || level_validated_children.size() >= candidates_per_level) {
                break;
            }
            candidate_attempts++;

            CandidatePatch patch = extract_candidate_patch(
                mesh_part, face_descriptors, face_normals, face_areas, edge_to_faces,
                is_handled_map, sc.dir, min_dot
            );
            if (!patch.is_valid || patch.faces.empty()) continue;

            if (!is_candidate_compatible_with_chain(
                patch.faces, edge_to_faces, is_handled_map, sc.dir, curr.draw_dirs
            )) continue;

            auto env_res = compute_exact_upper_envelope_mesh(
                mesh_part, face_descriptors, face_normals, is_handled_map,
                sc.dir, patch.faces, params.padding,
                /*override_tide=*/{},
                stock_box_mesh
            );
            if (env_res.solid_wedge.number_of_faces() == 0 || env_res.source_faces.empty()) continue;

            std::set<size_t> new_handled_faces = env_res.source_faces;
            for (auto f : patch.faces) {
                size_t f_idx = (size_t)f;
                if (face_normals[f_idx] * sc.dir == FT(0)) {
                    new_handled_faces.insert(f_idx);
                }
            }

            bool backdraft_in_source = false;
            for (size_t f_idx : new_handled_faces) {
                if (face_normals[f_idx] * sc.dir < min_dot) {
                    backdraft_in_source = true;
                    break;
                }
            }
            if (backdraft_in_source) continue;

            // Form candidate child node
            MoldChainNode child = curr;
            child.draw_dirs.push_back(sc.dir);
            child.solid_wedges.push_back(std::make_shared<const ExactMesh>(env_res.solid_wedge));
            child.piece_handled_faces.push_back(new_handled_faces);
            child.piece_boundary_loops.push_back(env_res.boundary_loops_3d);

            FT newly_handled_area = FT(0);
            for (size_t f_idx : new_handled_faces) {
                if (!child.is_handled[f_idx]) {
                    child.is_handled[f_idx] = true;
                    child.handled_count++;
                    newly_handled_area += face_areas[f_idx];
                }
            }
            if (newly_handled_area <= FT(0)) continue;

            child.unhandled_area = FT(0);
            for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
                if (!child.is_handled[f_idx]) {
                    child.unhandled_area += face_areas[f_idx];
                }
            }

            FT encroachment = sc.score.responsible_area - sc.score.net_score;
            child.total_score = curr.total_score + sc.score.net_score;
            child.energy = child.unhandled_area + lambda_pieces * FT(child.draw_dirs.size()) + encroachment;

            // Eager Level Validation & Progressive Stock Carving
            validations_count++;
            bool is_terminal = (child.unhandled_area <= FT(0) || child.handled_count == face_descriptors.size());
            ExactMesh validated_piece_mesh;

            if (is_terminal && curr.remaining_stock && !curr.remaining_stock->is_empty()) {
                // Section 7.5 & 7.6: Terminal Piece Complementation: P_K = B_{K-1}
                validated_piece_mesh = *curr.remaining_stock;
                MoldPiece term_piece{validated_piece_mesh, sc.dir, "terminal_piece", "#2bee2b", (int)child.solid_pieces.size() + 1};
                int backdraft_count = 0;
                if (!verify_piece_demoldability(term_piece, model_tree, params, &backdraft_count)) {
                    continue;
                }
                child.remaining_stock = nullptr; // Fully consumed with 0 scrap
            } else {
                if (!curr.remaining_stock || curr.remaining_stock->is_empty()) continue;

                // Carve piece directly from active remaining stock: P_k = B_{k-1} ∩ W_k
                ExactMesh carved;
                bool ok_inter = boolean::corefine_intersection(
                    *curr.remaining_stock, env_res.solid_wedge, carved,
                    params.kiss_mode, params.kiss_width, "remaining_stock ∩ wedge in beam search"
                );
                if (!ok_inter || carved.is_empty() || carved.number_of_faces() == 0 || !CGAL::is_closed(carved)) {
                    continue;
                }

                MoldPiece cand_piece{carved, sc.dir, "tentative_piece", "#2bee2b", (int)child.solid_pieces.size() + 1};
                int backdraft_count = 0;
                if (!verify_piece_demoldability(cand_piece, model_tree, params, &backdraft_count)) {
                    continue;
                }

                // Update residual stock for children: B_k = B_{k-1} \ P_k
                ExactMesh next_stock;
                bool ok_diff = boolean::corefine_difference(
                    *curr.remaining_stock, carved, next_stock,
                    params.kiss_mode, params.kiss_width, "remaining_stock \\ piece in beam search"
                );
                if (!ok_diff) continue;

                validated_piece_mesh = std::move(carved);
                child.remaining_stock = std::make_shared<const ExactMesh>(std::move(next_stock));
            }

            child.status = CandidateStatus::VALIDATED;
            child.solid_pieces.push_back(std::make_shared<const ExactMesh>(std::move(validated_piece_mesh)));

            // Log measured convergence
            std::cout << "    [BeamSearch] Level " << child.draw_dirs.size()
                      << " candidate along dir (" << CGAL::to_double(sc.dir.x())
                      << ", " << CGAL::to_double(sc.dir.y())
                      << ", " << CGAL::to_double(sc.dir.z())
                      << ") VALIDATED (#" << validations_count << "): handled " 
                      << child.handled_count << "/" << face_descriptors.size()
                      << " faces (remaining unhandled=" << CGAL::to_double(child.unhandled_area)
                      << " mm^2, delta_A=" << -CGAL::to_double(newly_handled_area) << " mm^2)."
                      << std::endl;

            if (child.unhandled_area <= FT(0) || child.handled_count == face_descriptors.size()) {
                level_validated_children.push_back(std::move(child));
                break;
            }

            level_validated_children.push_back(std::move(child));
        }

        // Insert validated children into global priority frontier
        for (auto& child : level_validated_children) {
            frontier.push_back(std::move(child));
        }

        // Sort frontier by energy ascending (lowest energy first, deeper chains favored)
        std::sort(frontier.begin(), frontier.end(), [](const auto& a, const auto& b) {
            bool a_comp = (a.unhandled_area <= FT(0));
            bool b_comp = (b.unhandled_area <= FT(0));
            if (a_comp != b_comp) return a_comp > b_comp;

            if (a.energy != b.energy) return a.energy < b.energy;
            if (a.unhandled_area != b.unhandled_area) return a.unhandled_area < b.unhandled_area;
            if (a.handled_count != b.handled_count) return a.handled_count > b.handled_count;
            if (a.draw_dirs.size() != b.draw_dirs.size()) return a.draw_dirs.size() > b.draw_dirs.size();
            return a.total_score > b.total_score;
        });

        if (frontier.size() > max_frontier_size) {
            frontier.resize(max_frontier_size);
        }
    }

    // Restore is_handled_map to initial state
    for (auto f : face_descriptors) {
        is_handled_map[f] = root.is_handled[f.idx()];
    }

    const auto& winner = found_complete ? best_complete : best_partial;
    bool complete = (winner.unhandled_area <= FT(0) || winner.handled_count == face_descriptors.size());
    std::vector<ExactMesh> final_pieces;
    final_pieces.reserve(winner.solid_pieces.size());
    for (const auto& ptr : winner.solid_pieces) {
        if (ptr) final_pieces.push_back(*ptr);
    }
    std::vector<ExactMesh> final_wedges;
    final_wedges.reserve(winner.solid_wedges.size());
    for (const auto& ptr : winner.solid_wedges) {
        if (ptr) final_wedges.push_back(*ptr);
    }
    return {
        winner.draw_dirs,
        final_pieces,
        final_wedges,
        winner.piece_handled_faces,
        winner.piece_boundary_loops,
        complete,
        winner.energy,
        winner.unhandled_area
    };
}

/**
 * @brief Convenience overload of decompose_mold_beam_search that initializes property maps automatically.
 */
inline MoldDecompositionResult decompose_mold_beam_search(
    ExactMesh& mesh_part,
    const std::map<EdgeKey, std::vector<int>>& edge_to_faces,
    const MoldParams& params,
    size_t max_pieces = 6,
    size_t candidates_per_level = 16,
    const std::vector<EK::Vector_3>& prior_draw_dirs = {},
    const std::vector<ExactMesh>& prior_solid_pieces = {},
    ExactMeshPtr initial_stock = nullptr
) {
    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<EK::Vector_3> face_normals(mesh_part.num_faces());
    std::vector<FT> face_areas(mesh_part.num_faces());
    FaceBoolMap is_handled_map = mesh_part.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

    for (auto f : mesh_part.faces()) {
        face_descriptors.push_back(f);
        size_t idx = f.idx();
        auto h = mesh_part.halfedge(f);
        auto p0 = mesh_part.point(mesh_part.source(h));
        auto p1 = mesh_part.point(mesh_part.target(h));
        auto p2 = mesh_part.point(mesh_part.target(mesh_part.next(h)));
        EK::Vector_3 raw_n = CGAL::normal(p0, p1, p2);
        FT len_sq = raw_n.squared_length();
        if (len_sq > FT(0)) {
            FT len = CGAL::approximate_sqrt(len_sq);
            face_normals[idx] = EK::Vector_3(raw_n.x() / len, raw_n.y() / len, raw_n.z() / len);
        } else {
            face_normals[idx] = raw_n;
        }
        face_areas[idx] = CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
    }

    return decompose_mold_beam_search(
        mesh_part, face_descriptors, face_normals, face_areas,
        edge_to_faces, is_handled_map, params,
        max_pieces, candidates_per_level, prior_draw_dirs, prior_solid_pieces,
        initial_stock
    );
}

} // namespace mold
} // namespace geo
} // namespace jotcad
