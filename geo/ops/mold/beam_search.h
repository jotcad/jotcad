#pragma once

#include "types.h"
#include "scoring.h"
#include "candidates.h"
#include "patch.h"
#include "compatibility.h"
#include "envelope.h"
#include "patch_dedup.h"
#include "realized_coverage.h"
#include "verify.h"
#include "stop_rule.h"
#include <CGAL/Polygon_mesh_processing/repair_degeneracies.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>

namespace jotcad {
namespace geo {
namespace mold {

inline std::string format_vec(const EK::Vector_3& v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "(%.2f, %.2f, %.2f)",
                  CGAL::to_double(v.x()), CGAL::to_double(v.y()), CGAL::to_double(v.z()));
    return std::string(buf);
}

inline std::string format_chain(const std::vector<EK::Vector_3>& dirs) {
    if (dirs.empty()) return "[]";
    std::string s = "[";
    for (size_t i = 0; i < dirs.size(); ++i) {
        if (i > 0) s += " -> ";
        s += format_vec(dirs[i]);
    }
    s += "]";
    return s;
}

/**
 * @brief Node representing an active candidate chain in the search frontier.
 */
struct MoldChainNode {
    std::shared_ptr<MoldChainNode> parent = nullptr;
    std::vector<EK::Vector_3> draw_dirs;
    std::vector<ExactMeshPtr> solid_wedges;
    std::vector<ExactMeshPtr> solid_pieces; // Realized and certified validated solid pieces
    ExactMeshPtr raw_stock = nullptr;       // Exact CSG residual stock: B_k = B_{k-1} \ P_k (KissMode::NONE)
    ExactMeshPtr clean_stock = nullptr;     // Kiss-resolved stock (params.kiss_mode) for downstream booleans
    std::vector<std::set<size_t>> piece_handled_faces;
    std::vector<std::vector<std::vector<Point_3>>> piece_boundary_loops;
    std::vector<bool> is_handled;
    std::vector<ExactMesh::Face_index> tentative_patch_faces;
    size_t handled_count = 0;
    FT unhandled_area = FT(0);
    FT energy = FT(0);
    FT total_score = FT(0);
    FT failure_penalty = FT(0);
    bool is_invalid = false;
    bool is_potential_terminal = false;

    FT effective_energy() const {
        if (is_potential_terminal) {
            const FT lambda_pieces(50);
            return lambda_pieces * FT(draw_dirs.size());
        }
        return energy;
    }
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
    size_t terminal_fallbacks = 0;
};

/**
 * @brief Memoized recursive stock accessor: ensures parent stock is carved, then carves this candidate on demand.
 */
inline ExactMeshPtr get_raw_stock(
    MoldChainNode* node,
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const MoldParams& params,
    const ExactMesh* stock_box_mesh,
    const Tree& model_tree,
    FaceBoolMap is_handled_map,
    const ExactMeshPtr& initial_stock,
    size_t& validations_count,
    std::map<VectorKey, EnvelopeMeshResult>& envelope_cache
);

/**
 * @brief Memoized clean stock accessor: resolves zero-volume kissing seams on demand
 * for nodes that will be used as parent stock in downstream booleans or terminal closure.
 */
inline ExactMeshPtr get_clean_stock(
    MoldChainNode* node,
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const MoldParams& params,
    const ExactMesh* stock_box_mesh,
    const Tree& model_tree,
    FaceBoolMap is_handled_map,
    const ExactMeshPtr& initial_stock,
    size_t& validations_count,
    std::map<VectorKey, EnvelopeMeshResult>& envelope_cache
) {
    if (!node) return initial_stock;
    if (node->clean_stock) return node->clean_stock; // Memoized clean stock hit!
    if (node->is_invalid) return nullptr;

    ExactMeshPtr raw = get_raw_stock(
        node, mesh_part, face_descriptors, face_normals, face_areas,
        params, stock_box_mesh, model_tree, is_handled_map, initial_stock, validations_count,
        envelope_cache
    );
    if (!raw || raw->is_empty()) return nullptr;

    if (params.kiss_mode == fix::KissMode::NONE) {
        node->clean_stock = raw;
        return node->clean_stock;
    }

    ExactMesh cleaned = *raw;
    std::cout << "      ↳ [Clean Stock] Resolving kissing seams on residual stock (mode=" 
              << (params.kiss_mode == fix::KissMode::WELD ? "WELD" : "PART") << ")... " << std::flush;
    auto t0 = std::chrono::steady_clock::now();
    boolean::regularize_and_resolve_kisses(cleaned, params.kiss_mode, params.kiss_width);
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "Done in " << std::chrono::duration<double, std::milli>(t1 - t0).count() << "ms." << std::endl << std::flush;

    node->clean_stock = std::make_shared<const ExactMesh>(std::move(cleaned));
    return node->clean_stock;
}

/**
 * @brief Memoized raw stock accessor: ensures clean parent stock, then carves this candidate with exact CSG (KissMode::NONE).
 */
inline ExactMeshPtr get_raw_stock(
    MoldChainNode* node,
    const ExactMesh& mesh_part,
    const std::vector<ExactMesh::Face_index>& face_descriptors,
    const std::vector<EK::Vector_3>& face_normals,
    const std::vector<FT>& face_areas,
    const MoldParams& params,
    const ExactMesh* stock_box_mesh,
    const Tree& model_tree,
    FaceBoolMap is_handled_map,
    const ExactMeshPtr& initial_stock,
    size_t& validations_count,
    std::map<VectorKey, EnvelopeMeshResult>& envelope_cache
) {
    if (!node) return initial_stock;
    if (node->raw_stock) return node->raw_stock; // Memoized cache hit!
    if (node->is_invalid) return nullptr;

    // 1. Ensure parent stock is clean of self-touches before we cut into it
    ExactMeshPtr parent_stock = get_clean_stock(
        node->parent.get(), mesh_part, face_descriptors, face_normals, face_areas,
        params, stock_box_mesh, model_tree, is_handled_map, initial_stock, validations_count,
        envelope_cache
    );
    if (!parent_stock || parent_stock->is_empty()) {
        node->is_invalid = true;
        return nullptr;
    }

    validations_count++;
    EK::Vector_3 cand_dir = node->draw_dirs.back();

    // Set up is_handled_map for the parent's state before this piece
    for (auto f : face_descriptors) {
        is_handled_map[f] = node->parent ? node->parent->is_handled[f.idx()] : false;
    }
    for (auto f : node->tentative_patch_faces) {
        is_handled_map[f] = false;
    }

    const auto& env_res = get_cached_envelope(
        cand_dir, mesh_part, face_descriptors, face_normals,
        params.padding, stock_box_mesh, envelope_cache
    );

    if (env_res.source_faces.empty()) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: empty upper envelope along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }

    FT min_dot(std::sin(CGAL::to_double(params.draft) * 2.0 * M_PI));
    const FT& dot_eps = mold_constants::zero_draft_dot_epsilon();
    FT eff_min_dot = (min_dot == FT(0)) ? -dot_eps : min_dot;

    for (size_t f_idx : env_res.source_faces) {
        FT dot = face_normals[f_idx] * cand_dir;
        if (dot < eff_min_dot) {
            std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: corridor face in backdraft along " << format_vec(cand_dir) << std::endl;
            node->is_invalid = true;
            return nullptr;
        }
    }

    // Lazy wedge construction: construct certified 3D solid wedge only on demand from cached envelope context
    VectorKey vkey{cand_dir.x(), cand_dir.y(), cand_dir.z()};
    auto& cached_res = envelope_cache[vkey];
    if (!cached_res.has_solid_wedge || cached_res.solid_wedge.is_empty()) {
        build_solid_wedge_from_context(cached_res, params.padding, stock_box_mesh);
    }
    if (cached_res.solid_wedge.number_of_faces() == 0) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: empty wedge along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }

    ExactMesh solid_wedge_mesh = cached_res.solid_wedge;
    auto boundary_loops_3d = cached_res.boundary_loops_3d;

    // Carve tentative piece from parent residual stock: P_k = B_{k-1} ∩ W_k
    ExactMesh validated_piece_mesh;
    bool ok_trim = boolean::corefine_intersection(
        *parent_stock, solid_wedge_mesh, validated_piece_mesh,
        fix::KissMode::NONE, params.kiss_width,
        "parent_stock ∩ wedge in beam search"
    );
    if (!ok_trim || validated_piece_mesh.is_empty() || !CGAL::is_closed(validated_piece_mesh)) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: CSG wedge trim failed or non-closed along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }
    validated_piece_mesh.collect_garbage();

    MoldPiece cand_piece{validated_piece_mesh, cand_dir, "tentative_piece", "#2bee2b", (int)node->draw_dirs.size()};
    int backdraft_count = 0;
    FT backdraft_area = FT(0);
    if (!verify_piece_demoldability(cand_piece, model_tree, params, &backdraft_count, &backdraft_area)) {
        node->is_invalid = true;
        return nullptr;
    }

    // Realized Forward-Progress Mandate:
    // A candidate piece is only physically viable if it captures newly handled cavity faces
    std::set<size_t> candidate_handled;
    if (!node->tentative_patch_faces.empty()) {
        for (auto f : node->tentative_patch_faces) {
            candidate_handled.insert(f.idx());
        }
    } else {
        candidate_handled = env_res.source_faces;
    }

    // Verify forward progress: piece MUST capture virgin (unhandled) cavity area
    // not already handled by its parent. Prune immediately to bypass expensive stock difference!
    FT newly_handled_area = FT(0);
    size_t newly_handled_count = 0;
    for (size_t f_idx : candidate_handled) {
        bool was_handled = node->parent ? node->parent->is_handled[f_idx] : false;
        if (!was_handled) {
            newly_handled_count++;
            newly_handled_area += face_areas[f_idx];
        }
    }
    if (newly_handled_count == 0 || newly_handled_area <= FT(1) / FT(1000)) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: zero virgin cavity area captured along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }

    // Subtract validated piece from parent residual stock: B_k = B_{k-1} \ P_k
    ExactMesh next_stock;
    bool ok_sub = boolean::corefine_difference(
        *parent_stock, validated_piece_mesh, next_stock,
        fix::KissMode::NONE, params.kiss_width,
        "residual_stock \\ piece in beam search"
    );
    if (!ok_sub) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: residual stock subtraction failed (corefine returned false) along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }
    if (!next_stock.is_empty() && !CGAL::is_closed(next_stock)) {
        std::cout << "      ↳ [Candidate #" << validations_count << "] Pruned: residual stock subtraction produced non-closed mesh along " << format_vec(cand_dir) << std::endl;
        node->is_invalid = true;
        return nullptr;
    }

    std::set<size_t> new_handled_faces = candidate_handled;

    if (node->parent) {
        node->is_handled = node->parent->is_handled;
        node->handled_count = node->parent->handled_count;
        node->unhandled_area = node->parent->unhandled_area;
    } else {
        node->is_handled.assign(face_descriptors.size(), false);
        node->handled_count = 0;
        node->unhandled_area = FT(0);
        for (auto a : face_areas) node->unhandled_area += a;
    }

    for (size_t f_idx : candidate_handled) {
        if (!node->is_handled[f_idx]) {
            node->is_handled[f_idx] = true;
            node->handled_count++;
            node->unhandled_area = (node->unhandled_area > face_areas[f_idx]) ? (node->unhandled_area - face_areas[f_idx]) : FT(0);
        }
    }
    const FT lambda_pieces(50);
    node->energy = node->unhandled_area + lambda_pieces * FT(node->draw_dirs.size());

    if (node->parent && node->solid_pieces.empty()) {
        node->solid_pieces = node->parent->solid_pieces;
        node->solid_wedges = node->parent->solid_wedges;
        node->piece_handled_faces = node->parent->piece_handled_faces;
        node->piece_boundary_loops = node->parent->piece_boundary_loops;
    }

    node->solid_wedges.push_back(std::make_shared<const ExactMesh>(solid_wedge_mesh));
    node->solid_pieces.push_back(std::make_shared<const ExactMesh>(std::move(validated_piece_mesh)));
    node->raw_stock = std::make_shared<const ExactMesh>(std::move(next_stock));
    node->clean_stock = nullptr; // Lazily generated when needed for downstream booleans
    node->piece_handled_faces.push_back(std::move(new_handled_faces));
    node->piece_boundary_loops.push_back(boundary_loops_3d);

    double delta_unhandled = CGAL::to_double(node->parent ? (node->parent->unhandled_area - node->unhandled_area) : FT(0));
    std::cout << "      ↳ [CARVED & VALIDATED #" << validations_count << "] Piece #" << node->draw_dirs.size()
              << " along " << format_vec(cand_dir) << " | Handled " << node->handled_count << "/" << face_descriptors.size()
              << " faces | Δ Area: -" << std::fixed << std::setprecision(1) << delta_unhandled << " mm²\n"
              << "         Remaining Unhandled: " << CGAL::to_double(node->unhandled_area) 
              << " mm² | Energy: " << CGAL::to_double(node->energy) << " mm²"
              << std::endl << std::flush;

    return node->raw_stock;
}

/**
 * @brief Orchestrates multi-piece mold decomposition as a Priority-Driven Search with Memoized Stock Carving.
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
    const ExactMesh* stock_box_mesh = nullptr,
    SearchVisitor* visitor = nullptr
) {
    FT min_dot(std::sin(CGAL::to_double(params.draft) * 2.0 * M_PI));
    const FT& dot_eps = mold_constants::zero_draft_dot_epsilon();
    FT eff_min_dot = (min_dot == FT(0)) ? -dot_eps : min_dot;
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
    const FT lambda_pieces = params.policy.lambda_pieces; // Regularizer penalty per piece
    root.energy = root.unhandled_area + lambda_pieces * FT(root.draw_dirs.size());
    size_t terminal_fallbacks_count = 0;

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
    root.raw_stock = active_stock;
    root.clean_stock = active_stock;

    // If initial state is already 100% complete, return certified complete
    if (root.handled_count == face_descriptors.size() || root.unhandled_area <= FT(0)) {
        return {prior_draw_dirs, prior_solid_pieces, {}, {}, {}, true, root.energy, FT(0)};
    }

    // Global Priority Frontier across all search depths (stores exclusively VALIDATED candidates)
    std::vector<MoldChainNode> frontier = { root };
    const size_t max_frontier_size = params.policy.frontier_bound;
    size_t validations_count = 0;
    std::map<VectorKey, EnvelopeMeshResult> envelope_cache;

    MoldChainNode best_complete;
    bool found_complete = false;
    MoldChainNode best_partial = root;

    ConvergenceStopRule default_stop_rule(params.stopping);
    SearchVisitor& active_visitor = visitor ? *visitor : default_stop_rule;

    std::cout << "    [BeamSearch] Starting Level-by-Level Priority Search (frontier_bound=" << max_frontier_size 
              << ", max_pieces=" << max_pieces << ", prior_pieces=" << prior_draw_dirs.size()
              << ", unhandled_area=" << CGAL::to_double(root.unhandled_area) 
              << " mm^2)..." << std::endl;

    size_t iter = 0;
    while (!frontier.empty()) {
        iter++;

        SearchProgress progress;
        progress.iteration = iter;
        progress.validations_count = validations_count;
        progress.frontier_size = frontier.size();
        progress.found_complete = found_complete;
        progress.best_complete_pieces = found_complete ? best_complete.draw_dirs.size() : 0;
        progress.best_complete_energy = found_complete ? best_complete.effective_energy() : FT(-1);
        progress.current_pieces = frontier.front().draw_dirs.size();
        progress.current_energy = frontier.front().effective_energy();
        progress.min_frontier_energy = frontier.front().effective_energy();

        std::string stop_reason;
        if (active_visitor.should_terminate(progress, &stop_reason)) {
            std::cout << "    [BeamSearch] Termination policy triggered: " << stop_reason << std::endl;
            break;
        }

        active_visitor.on_iteration_start(progress);

        // Pop lowest-energy candidate node from frontier
        MoldChainNode curr = std::move(frontier.front());
        frontier.erase(frontier.begin());

        std::cout << "\n    [Frontier Iter #" << iter << " | Queue: " << frontier.size() + 1 
                  << "] Expanding Level " << curr.draw_dirs.size()
                  << " | Path: " << format_chain(curr.draw_dirs)
                  << " | Energy: " << CGAL::to_double(curr.effective_energy()) << " mm²"
                  << std::endl << std::flush;

        // 1. Selection Gate: Can this candidate achieve certified complete decomposition?
        bool is_terminal_claimed = (curr.unhandled_area <= FT(0) || curr.handled_count == face_descriptors.size() || curr.is_potential_terminal);
        if (params.policy.enable_terminal_closure && is_terminal_claimed) {
            ExactMeshPtr parent_stock = get_clean_stock(
                curr.parent.get(), mesh_part, face_descriptors, face_normals, face_areas,
                params, stock_box_mesh, model_tree, is_handled_map, active_stock, validations_count,
                envelope_cache
            );
            if (parent_stock && !parent_stock->is_empty()) {
                EK::Vector_3 cand_dir = curr.draw_dirs.back();
                MoldPiece cand_piece{*parent_stock, cand_dir, "terminal_piece", "#2bee2b", (int)curr.draw_dirs.size()};
                int backdraft_count = 0;
                FT backdraft_area = FT(0);
                if (verify_piece_demoldability(cand_piece, model_tree, params, &backdraft_count, &backdraft_area)) {
                    if (curr.parent) {
                        curr.solid_pieces = curr.parent->solid_pieces;
                        curr.solid_wedges = curr.parent->solid_wedges;
                        curr.piece_handled_faces = curr.parent->piece_handled_faces;
                        curr.piece_boundary_loops = curr.parent->piece_boundary_loops;
                    }
                    std::set<size_t> new_handled_faces;
                    std::set<size_t> previously_handled;
                    if (curr.parent) {
                        for (const auto& p_faces : curr.parent->piece_handled_faces) {
                            previously_handled.insert(p_faces.begin(), p_faces.end());
                        }
                    }
                    for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
                        if (previously_handled.find(f_idx) == previously_handled.end()) {
                            new_handled_faces.insert(f_idx);
                        }
                    }
                    curr.solid_wedges.push_back(parent_stock);
                    curr.solid_pieces.push_back(parent_stock);
                    curr.raw_stock = nullptr; // Residual stock fully consumed
                    curr.clean_stock = nullptr;
                    curr.piece_handled_faces.push_back(std::move(new_handled_faces));
                    curr.piece_boundary_loops.push_back({});
                    curr.tentative_patch_faces.clear();

                    curr.handled_count = face_descriptors.size();
                    curr.unhandled_area = FT(0);
                    std::fill(curr.is_handled.begin(), curr.is_handled.end(), true);
                    curr.is_potential_terminal = false;
                    curr.energy = lambda_pieces * FT(curr.draw_dirs.size());

                    std::cout << "    [BeamSearch] Level " << curr.draw_dirs.size()
                              << " TERMINAL piece along dir (" << CGAL::to_double(cand_dir.x())
                              << ", " << CGAL::to_double(cand_dir.y())
                              << ", " << CGAL::to_double(cand_dir.z())
                              << ") VALIDATED: directly took remaining stock (handled "
                              << curr.handled_count << "/" << face_descriptors.size()
                              << " faces, energy=" << CGAL::to_double(curr.energy) << ")."
                              << std::endl << std::flush;

                    std::cout << "    [BeamSearch] Certified complete decomposition found with "
                              << curr.draw_dirs.size() << " pieces! (Energy=" << CGAL::to_double(curr.energy) << ")" << std::endl;
                    if (!found_complete || curr.energy < best_complete.energy) {
                        best_complete = std::move(curr);
                        found_complete = true;
                    }
                    progress.found_complete = true;
                    progress.best_complete_pieces = best_complete.draw_dirs.size();
                    progress.best_complete_energy = best_complete.effective_energy();
                    active_visitor.on_complete_found(progress);

                    if (active_visitor.should_terminate(progress, &stop_reason)) {
                        std::cout << "    [BeamSearch] Termination policy triggered: " << stop_reason << std::endl;
                        break;
                    }
                    continue;
                } else {
                    // Terminal closure along cand_dir failed because parent_stock cannot be extracted as a single block.
                    // Fall back seamlessly to an intermediate progressive piece (do NOT prune)!
                    std::cout << "    [BeamSearch] Level " << curr.draw_dirs.size()
                              << " terminal closure along (" << CGAL::to_double(cand_dir.x())
                              << ", " << CGAL::to_double(cand_dir.y())
                              << ", " << CGAL::to_double(cand_dir.z())
                              << ") has " << backdraft_count << " undercuts (" << CGAL::to_double(backdraft_area)
                              << " mm^2). Falling back to progressive intermediate carving..." << std::endl << std::flush;
                    curr.is_potential_terminal = false;
                    terminal_fallbacks_count++;
                }
            }
        }

        // If max pieces reached for this chain, cannot derive further
        if (curr.draw_dirs.size() >= max_pieces) {
            continue;
        }

        // 2. Carve curr's latest piece ON DEMAND via get_raw_stock as an intermediate piece
        ExactMeshPtr curr_stock = get_raw_stock(
            &curr, mesh_part, face_descriptors, face_normals, face_areas,
            params, stock_box_mesh, model_tree, is_handled_map, active_stock, validations_count,
            envelope_cache
        );
        if (!curr_stock) {
            continue; // Discard (prune) if carving or demoldability failed
        }

        // Check if carving this intermediate piece completed all remaining faces!
        if (curr.unhandled_area <= FT(0) || curr.handled_count == face_descriptors.size()) {
            std::cout << "    [BeamSearch] Certified complete decomposition found with "
                      << curr.draw_dirs.size() << " pieces via envelope carving! (Energy="
                      << CGAL::to_double(curr.energy) << ")" << std::endl;
            if (!found_complete || curr.energy < best_complete.energy) {
                best_complete = std::move(curr);
                found_complete = true;
            }
            progress.found_complete = true;
            progress.best_complete_pieces = best_complete.draw_dirs.size();
            progress.best_complete_energy = best_complete.effective_energy();
            active_visitor.on_complete_found(progress);

            if (active_visitor.should_terminate(progress, &stop_reason)) {
                std::cout << "    [BeamSearch] Termination policy triggered: " << stop_reason << std::endl;
                break;
            }
            continue;
        }

        // 3. Track best validated partial solution
        if (curr.handled_count > best_partial.handled_count || 
           (curr.handled_count == best_partial.handled_count && curr.energy < best_partial.energy)) {
            best_partial = curr;
        }

        // 4. Derivation Gate: Generate large pool of TENTATIVE children from validated parent
        for (auto f : face_descriptors) {
            is_handled_map[f] = curr.is_handled[f.idx()];
        }

        auto candidate_dirs = generate_candidate_directions(
            mesh_part, face_descriptors, face_normals, face_areas,
            is_handled_map, curr.draw_dirs, /*num_exploratory=*/0
        );

        auto parent_ptr = std::make_shared<MoldChainNode>(std::move(curr));

        std::vector<ScoredCandidate> scored_cands;

        // Cheap normal-only prediction: used ONLY to rank and deduplicate, never to credit coverage.
        auto t_pred0 = std::chrono::steady_clock::now();
        for (const auto& d : candidate_dirs) {
            CandidatePatch patch = extract_candidate_patch(
                mesh_part, face_descriptors, face_normals, face_areas, edge_to_faces,
                is_handled_map, d, eff_min_dot
            );
            if (!patch.is_valid || patch.faces.empty()) continue;

            if (!is_candidate_compatible_with_chain(
                patch.faces, edge_to_faces, is_handled_map, d, parent_ptr->draw_dirs
            )) continue;

            FT virgin_area = FT(0);
            FT total_patch_area = FT(0);
            for (auto f : patch.faces) {
                size_t f_idx = (size_t)f.idx();
                FT a = face_areas[f_idx];
                total_patch_area += a;
                if (!parent_ptr->is_handled[f_idx]) {
                    virgin_area += a;
                }
            }

            // Forward Progress Mandate: must handle virgin cavity area
            if (virgin_area <= FT(1) / FT(1000)) continue;

            // Majority Unexplored Mandate: virgin_area / total_patch_area >= 0.5
            if (total_patch_area > FT(0) && (virgin_area * FT(2) < total_patch_area)) {
                continue;
            }

            scored_cands.push_back({d, std::move(patch), virgin_area, total_patch_area, FT(0)});
        }
        auto t_pred1 = std::chrono::steady_clock::now();
        double pred_ms = std::chrono::duration<double, std::milli>(t_pred1 - t_pred0).count();
        std::cout << "      ↳ [Predict] " << candidate_dirs.size() << " directions in " << pred_ms << " ms ("
                  << (candidate_dirs.empty() ? 0.0 : pred_ms / candidate_dirs.size()) << " ms/direction)." << std::endl;

        // Sort descending by virgin responsible area (largest new patch first)
        std::sort(scored_cands.begin(), scored_cands.end(), [](const auto& a, const auto& b) {
            return a.virgin_area > b.virgin_area;
        });

        if (!scored_cands.empty()) {
            std::cout << "      ↳ [Expansion] Evaluated " << scored_cands.size() << " raw viable candidates." << std::endl;
        }

        // Deduplicate candidates targeting the same physical feature patch via Jaccard similarity
        scored_cands = deduplicate_candidate_patches(
            scored_cands, face_normals, face_areas, parent_ptr->is_handled,
            params.policy.jaccard_threshold
        );

        // Realized coverage: replace predicted patches with CGAL::upper_envelope_3 visible faces.
        // From here on, is_handled / energy / terminal claims are driven by occlusion-aware coverage.
        scored_cands = realize_candidate_coverage(
            scored_cands, mesh_part, face_descriptors, face_normals, face_areas,
            parent_ptr->is_handled, params.padding, stock_box_mesh, envelope_cache,
            params.policy
        );

        if (!scored_cands.empty()) {
            std::cout << "      ↳ [Expansion] Queued " << scored_cands.size() << " unique feature champions. "
                      << "Top child: " << format_vec(scored_cands[0].dir)
                      << " (new area=" << CGAL::to_double(scored_cands[0].virgin_area) << " mm²)" << std::endl;
        } else {
            std::cout << "      ↳ [Expansion] 0 viable candidate directions from this branch." << std::endl;
        }

        // Insert physically viable TENTATIVE children into frontier (governed by energy dominance)
        for (auto& sc : scored_cands) {
            MoldChainNode child;
            child.parent = parent_ptr;
            child.draw_dirs = parent_ptr->draw_dirs;
            child.draw_dirs.push_back(sc.dir);
            child.tentative_patch_faces = std::move(sc.patch.faces);
            child.raw_stock = nullptr; // Uncarved: evaluated on demand
            child.clean_stock = nullptr;
            child.is_handled = parent_ptr->is_handled;
            child.handled_count = parent_ptr->handled_count;
            child.unhandled_area = parent_ptr->unhandled_area;
            child.total_score = sc.virgin_area;

            FT newly_handled_area = FT(0);
            for (auto f : child.tentative_patch_faces) {
                size_t f_idx = (size_t)f.idx();
                if (!child.is_handled[f_idx]) {
                    child.is_handled[f_idx] = true;
                    child.handled_count++;
                    newly_handled_area += face_areas[f_idx];
                }
            }
            child.unhandled_area = (child.unhandled_area > newly_handled_area) ? (child.unhandled_area - newly_handled_area) : FT(0);
            if (child.unhandled_area <= params.policy.unhandled_area_zero_epsilon) {
                child.unhandled_area = FT(0);
            }

            // Under RESIDUAL_STOCK_CONTACT: if all remaining unhandled faces are non-backdrafting (n * d >= 0)
            // along sc.dir, this candidate can act as the terminal piece consuming residual stock.
            if (params.policy.certification == CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT && child.parent) {
                bool all_remaining_releasable = true;
                for (size_t f_idx = 0; f_idx < face_descriptors.size(); ++f_idx) {
                    if (!child.is_handled[f_idx]) {
                        if (face_normals[f_idx] * sc.dir < FT(0)) {
                            all_remaining_releasable = false;
                            break;
                        }
                    }
                }
                if (all_remaining_releasable) {
                    child.is_potential_terminal = true;
                }
            }
            child.energy = child.unhandled_area + lambda_pieces * FT(child.draw_dirs.size());

            frontier.push_back(std::move(child));
        }

        // 5. Sort frontier by effective energy ascending (lowest energy / complete candidates first)
        std::sort(frontier.begin(), frontier.end(), [](const auto& a, const auto& b) {
            FT a_eff = a.effective_energy();
            FT b_eff = b.effective_energy();
            if (a_eff != b_eff) return a_eff < b_eff;

            bool a_comp = (a.unhandled_area <= FT(0) || a.is_potential_terminal);
            bool b_comp = (b.unhandled_area <= FT(0) || b.is_potential_terminal);
            if (a_comp != b_comp) return a_comp > b_comp;

            // Prefer already carved over uncarved if energy is equal
            bool a_carved = (a.raw_stock != nullptr);
            bool b_carved = (b.raw_stock != nullptr);
            if (a_carved != b_carved) return a_carved > b_carved;
            if (a.unhandled_area != b.unhandled_area) return a.unhandled_area < b.unhandled_area;
            if (a.handled_count != b.handled_count) return a.handled_count > b.handled_count;
            if (a.draw_dirs.size() != b.draw_dirs.size()) return a.draw_dirs.size() > b.draw_dirs.size();
            return a.total_score > b.total_score;
        });

        if (frontier.size() > max_frontier_size) {
            frontier.resize(max_frontier_size);
        }

        FT best_front_energy = frontier.empty() ? FT(0) : frontier.front().effective_energy();
        std::cout << "      ↳ [Frontier Status] Queue: " << frontier.size()
                  << " | Min Energy: " << CGAL::to_double(best_front_energy)
                  << " mm² | Certified Bound: " 
                  << (found_complete ? (std::to_string((int)CGAL::to_double(best_complete.energy)) + " mm²") : "NONE")
                  << std::endl << std::flush;

        progress.frontier_size = frontier.size();
        progress.min_frontier_energy = best_front_energy;
        active_visitor.on_frontier_updated(progress);

        if (active_visitor.should_terminate(progress, &stop_reason)) {
            std::cout << "    [BeamSearch] Termination policy triggered: " << stop_reason << std::endl;
            break;
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
        winner.unhandled_area,
        terminal_fallbacks_count
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
    ExactMeshPtr initial_stock = nullptr,
    SearchVisitor* visitor = nullptr
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
        face_normals[idx] = CGAL::normal(p0, p1, p2);
        face_areas[idx] = CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
    }

    return decompose_mold_beam_search(
        mesh_part, face_descriptors, face_normals, face_areas,
        edge_to_faces, is_handled_map, params,
        max_pieces, candidates_per_level, prior_draw_dirs, prior_solid_pieces,
        initial_stock, nullptr, visitor
    );
}

} // namespace mold
} // namespace geo
} // namespace jotcad
