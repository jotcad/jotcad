#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/beam_search.h"
#include "mold/policy.h"
#include "mold/realized_coverage.h"
#include "boolean/engine.h"
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_policy_test");
    register_all_ops(&vfs);

    std::cout << "========================================================\n"
              << "Testing Mold Decomposition Policy Suite (mold/policy.h)\n"
              << "========================================================" << std::endl;

    // Build benchmark 3D Box (20 x 20 x 10)
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 20.0;
    box_sel.parameters["height"] = 20.0;
    box_sel.parameters["depth"] = 10.0;
    box_sel.output = "$out";

    Processor::execute(&vfs, box_sel);
    Shape box_shape = vfs.read<Shape>(box_sel);
    assert(box_shape.is_real() && box_shape.geometry.has_value());

    Geometry box_geo = vfs.read<Geometry>(*box_shape.geometry);
    ExactMesh mesh = boolean::Engine::geometry_to_mesh(box_geo);

    std::map<EdgeKey, std::vector<int>> edge_to_faces;
    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<EK::Vector_3> face_normals(mesh.num_faces());
    std::vector<FT> face_areas(mesh.num_faces());

    for (auto f : mesh.faces()) {
        size_t idx = f.idx();
        face_descriptors.push_back(f);
        auto h = mesh.halfedge(f);
        for (int i = 0; i < 3; ++i) {
            int u = (int)mesh.source(h);
            int v = (int)mesh.target(h);
            if (u > v) std::swap(u, v);
            edge_to_faces[{u, v}].push_back((int)idx);
            h = mesh.next(h);
        }
        auto p0 = mesh.point(mesh.source(h));
        auto p1 = mesh.point(mesh.target(h));
        auto p2 = mesh.point(mesh.target(mesh.next(h)));
        face_normals[idx] = CGAL::normal(p0, p1, p2);
        face_areas[idx] = CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
    }

    FaceBoolMap is_handled_map = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;
    std::vector<bool> no_parent_handled(mesh.num_faces(), false);

    // =========================================================================
    // Part 1: Sub-millisecond Candidate Scoring Verification
    // =========================================================================
    std::cout << "\n--- Part 1: Candidate Scoring Verification across Wall Weights ---\n";
    {
        EK::Vector_3 dir_z(FT(0), FT(0), FT(1));
        EK::Vector_3 dir_x(FT(1), FT(0), FT(0));

        // Unit diagonal (1, 0, 1) / sqrt(2) on S^2
        double d_len = std::sqrt(2.0);
        EK::Vector_3 dir_diag(FT(1.0 / d_len), FT(0), FT(1.0 / d_len));

        // Unit corner (1, 1, 1) / sqrt(3) on S^2
        double c_len = std::sqrt(3.0);
        EK::Vector_3 dir_corner(FT(1.0 / c_len), FT(1.0 / c_len), FT(1.0 / c_len));

        std::vector<EK::Vector_3> test_dirs = {dir_z, dir_x, dir_diag, dir_corner};
        std::vector<std::string> dir_names = {"+Z (0,0,1)", "+X (1,0,0)", "Diag (1,0,1)", "Corner (1,1,1)"};
        std::vector<double> test_weights = {0.0, 0.25, 0.5, 1.0};

        ExactMesh stock_box = boolean::Engine::geometry_to_mesh(
            build_box_geo(FT(-15), FT(15), FT(-15), FT(15), FT(-10), FT(10))
        );
        EnvelopeCache env_cache;

        for (double w : test_weights) {
            std::cout << "\n  [Scoring Test: Wall Weight w = " << std::fixed << std::setprecision(2) << w << "]\n";
            MoldDecompositionPolicy policy;
            policy.vertical_walls = VerticalWallPolicy::ALL_IN_PATCH;
            policy.vertical_wall_weight = FT(w);

            std::map<std::string, double> dir_scores;
            for (size_t i = 0; i < test_dirs.size(); ++i) {
                const auto& d = test_dirs[i];
                CandidatePatch patch = extract_candidate_patch(
                    mesh, face_descriptors, face_normals, face_areas, edge_to_faces,
                    is_handled_map, d, FT(0)
                );

                std::vector<ScoredCandidate> champions = {{d, std::move(patch), FT(0), FT(0), FT(0)}};
                auto realized = realize_candidate_coverage(
                    champions, mesh, face_descriptors, face_normals, face_areas,
                    no_parent_handled, FT(5), &stock_box, env_cache, policy
                );

                double scored_area = realized.empty() ? 0.0 : CGAL::to_double(realized[0].virgin_area);
                dir_scores[dir_names[i]] = scored_area;
                std::cout << "    " << std::setw(15) << std::left << dir_names[i] 
                          << " -> Score: " << std::fixed << std::setprecision(1) << scored_area << " mm²\n";
            }

            // Assert mathematical hierarchy across weights
            double s_z = dir_scores["+Z (0,0,1)"];
            double s_x = dir_scores["+X (1,0,0)"];
            double s_diag = dir_scores["Diag (1,0,1)"];
            double s_corner = dir_scores["Corner (1,1,1)"];

            if (w == 0.0) {
                assert(s_corner > s_diag && s_diag > s_z && s_z > s_x && "At w=0, Corner > Diag > +Z > +X");
            } else if (w == 0.25) {
                assert(s_corner > s_diag && s_diag > s_z && s_z > s_x && "At w=0.25, Corner > Diag > +Z > +X");
            } else if (w == 0.5) {
                assert(std::abs(s_corner - s_diag) < 1e-4 && std::abs(s_diag - s_z) < 1e-4 && std::abs(s_z - s_x) < 1e-4 && "At w=0.5, all directions tie");
            } else if (w == 1.0) {
                assert(s_x > s_z && s_z > s_diag && s_diag > s_corner && "At w=1.0, +X > +Z > Diag > Corner");
            }
        }
    }

    // =========================================================================
    // Part 2: Policy Sweep with Full Beam Search
    // =========================================================================
    std::cout << "\n--- Part 2: Policy Sweep with Full Beam Search ---\n";

    auto run_sweep = [&](const std::string& name, const MoldDecompositionPolicy& policy) {
        std::cout << "\n  >> Running Sweep: " << name << " <<\n";
        MoldParams params;
        params.draft = FT(0);
        params.padding = FT(5);
        params.policy = policy;

        auto result = decompose_mold_beam_search(
            mesh, edge_to_faces, params, /*max_pieces=*/4, /*candidates_per_level=*/4
        );

        std::cout << "     Pieces: " << result.draw_dirs.size()
                  << " | Complete: " << (result.is_complete ? "YES" : "NO")
                  << " | Remaining Area: " << CGAL::to_double(result.remaining_unhandled_area) << " mm²\n";
        std::cout << "     Draw Directions:\n";
        for (size_t p = 0; p < result.draw_dirs.size(); ++p) {
            const auto& d = result.draw_dirs[p];
            std::cout << "       Piece #" << (p + 1) << ": (" 
                      << CGAL::to_double(d.x()) << ", " << CGAL::to_double(d.y()) << ", " << CGAL::to_double(d.z()) << ")\n";
        }

        // Telemetry & Verification (Metadata vs Physical Reality)
        Tree model_tree(mesh.faces().begin(), mesh.faces().end(), mesh);
        double total_physical_contact = 0.0;
        std::set<size_t> all_handled_union;

        for (size_t p = 0; p < result.solid_pieces.size(); ++p) {
            FT contact_area = FT(0);
            for (auto f : result.solid_pieces[p].faces()) {
                auto h = result.solid_pieces[p].halfedge(f);
                auto p0 = result.solid_pieces[p].point(result.solid_pieces[p].source(h));
                auto p1 = result.solid_pieces[p].point(result.solid_pieces[p].target(h));
                auto p2 = result.solid_pieces[p].point(result.solid_pieces[p].target(result.solid_pieces[p].next(h)));
                EK::Point_3 mid((p0.x() + p1.x() + p2.x()) / FT(3), (p0.y() + p1.y() + p2.y()) / FT(3), (p0.z() + p1.z() + p2.z()) / FT(3));
                if (model_tree.squared_distance(mid) < FT(1) / FT(10000)) {
                    contact_area += CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
                }
            }
            double phys_area = CGAL::to_double(contact_area);
            total_physical_contact += phys_area;

            // Verify demoldability for each carved piece
            MoldPiece mp{result.solid_pieces[p], result.draw_dirs[p], "piece", "#ffffff", (int)p};
            int backdraft_count = 0;
            FT backdraft_area = FT(0);
            bool is_demoldable = verify_piece_demoldability(mp, model_tree, params, &backdraft_count, &backdraft_area);
            assert(is_demoldable && backdraft_count == 0 && "Piece must demold with zero backdrafts");

            // Compute metadata claimed area from piece_handled_faces
            double meta_area = 0.0;
            if (p < result.piece_handled_faces.size()) {
                for (size_t f_idx : result.piece_handled_faces[p]) {
                    meta_area += CGAL::to_double(face_areas[f_idx]);
                    all_handled_union.insert(f_idx);
                }
            }
            std::cout << "       Piece #" << (p + 1) << " -> Claimed Area: " 
                      << std::fixed << std::setprecision(1) << meta_area << " mm² | Physical Contact: "
                      << phys_area << " mm² | Demoldable: YES (0 backdrafts)\n";
        }
        std::cout << "     Total Physical Model Surface Covered: " 
                  << std::fixed << std::setprecision(1) << total_physical_contact << " / 1600.0 mm²\n";

        if (policy.certification == CoverageCertificationPolicy::RESIDUAL_STOCK_CONTACT) {
            assert(result.is_complete && "RESIDUAL_STOCK_CONTACT must achieve complete decomposition");
            assert(result.draw_dirs.size() == 2 && "RESIDUAL_STOCK_CONTACT must achieve 2-piece lower bound");
            assert(total_physical_contact >= 1599.9 && "Pieces must physically contact 100% of model surface");
            assert(all_handled_union.size() == face_descriptors.size() && "All model faces must be accounted for in piece_handled_faces");
            // Check that claimed area matches physical contact area without phantom inflation!
            for (size_t p = 0; p < result.solid_pieces.size(); ++p) {
                double meta_area = 0.0;
                for (size_t f_idx : result.piece_handled_faces[p]) meta_area += CGAL::to_double(face_areas[f_idx]);
                FT piece_contact = FT(0);
                for (auto f : result.solid_pieces[p].faces()) {
                    auto h = result.solid_pieces[p].halfedge(f);
                    auto p0 = result.solid_pieces[p].point(result.solid_pieces[p].source(h));
                    auto p1 = result.solid_pieces[p].point(result.solid_pieces[p].target(h));
                    auto p2 = result.solid_pieces[p].point(result.solid_pieces[p].target(result.solid_pieces[p].next(h)));
                    EK::Point_3 mid((p0.x() + p1.x() + p2.x()) / FT(3), (p0.y() + p1.y() + p2.y()) / FT(3), (p0.z() + p1.z() + p2.z()) / FT(3));
                    if (model_tree.squared_distance(mid) < FT(1) / FT(10000)) {
                        piece_contact += CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
                    }
                }
                assert(std::abs(meta_area - CGAL::to_double(piece_contact)) < 0.1 && "Under RESIDUAL_STOCK_CONTACT, claimed area must equal physical contact area!");
            }
        } else if (policy.vertical_walls == VerticalWallPolicy::ALL_IN_PATCH) {
            assert(result.is_complete && "ALL_IN_PATCH must produce a complete mold");
            assert(result.draw_dirs.size() == 2 && "Box decomposition must achieve 2-piece lower bound");
            assert(total_physical_contact >= 1599.9 && "Pieces must physically contact 100% of model surface");
            assert(all_handled_union.size() == face_descriptors.size() && "All model faces must be accounted for in piece_handled_faces");
        } else {
            // Note: Under the baseline EXCLUDE policy, intermediate wedges cannot claim vertical walls.
            // As a result, the solver is blind to the 2-piece decomposition and degrades into 3 pieces
            // through oblique directions, but claimed area matches physical contact for every piece.
            assert(result.is_complete && "EXCLUDE baseline completes via oblique directions");
            assert(result.draw_dirs.size() == 3 && "EXCLUDE baseline degrades to 3 pieces on a box");
            assert(total_physical_contact >= 1599.9 && "Pieces must physically contact 100% of model surface");
        }
    };

    // Run A: Baseline EXCLUDE
    {
        MoldDecompositionPolicy p = MoldDecompositionPolicy::current();
        run_sweep("EXCLUDE (Baseline)", p);
    }

    // Run B: ALL_IN_PATCH with w = 1.0 (Full wall weight)
    {
        MoldDecompositionPolicy p = MoldDecompositionPolicy::legacy_patch();
        p.vertical_wall_weight = FT(1);
        run_sweep("ALL_IN_PATCH (w = 1.0)", p);
    }

    // Run C: ALL_IN_PATCH with w = 0.25 (Discounted wall weight)
    {
        MoldDecompositionPolicy p = MoldDecompositionPolicy::legacy_patch();
        p.vertical_wall_weight = FT(1) / FT(4);
        run_sweep("ALL_IN_PATCH (w = 0.25)", p);
    }

    // Run D: Analytical Envelope with RESIDUAL_STOCK_CONTACT
    {
        MoldDecompositionPolicy p = MoldDecompositionPolicy::analytical_envelope();
        run_sweep("RESIDUAL_STOCK_CONTACT (Analytical 2-Piece & 1:1 Contact)", p);
    }

    std::cout << "\n========================================================\n"
              << "Mold Policy Test Suite Completed Successfully!\n"
              << "========================================================" << std::endl;
    return 0;
}
