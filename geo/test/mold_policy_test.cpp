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
#include <numeric>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

struct MeshAnalysis {
    std::map<EdgeKey, std::vector<int>> edge_to_faces;
    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<EK::Vector_3> face_normals;
    std::vector<FT> face_areas;
};

inline MeshAnalysis analyze_mesh(const ExactMesh& mesh) {
    MeshAnalysis a;
    a.face_normals.resize(mesh.num_faces());
    a.face_areas.resize(mesh.num_faces());
    for (auto f : mesh.faces()) {
        size_t idx = f.idx();
        a.face_descriptors.push_back(f);
        auto h = mesh.halfedge(f);
        for (int i = 0; i < 3; ++i) {
            int u = (int)mesh.source(h);
            int v = (int)mesh.target(h);
            if (u > v) std::swap(u, v);
            a.edge_to_faces[{u, v}].push_back((int)idx);
            h = mesh.next(h);
        }
        auto p0 = mesh.point(mesh.source(h));
        auto p1 = mesh.point(mesh.target(h));
        auto p2 = mesh.point(mesh.target(mesh.next(h)));
        a.face_normals[idx] = CGAL::normal(p0, p1, p2);
        a.face_areas[idx] = CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
    }
    return a;
}

int main() {
    std::cout << std::unitbuf;
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

    MeshAnalysis box_analysis = analyze_mesh(mesh);
    const auto& edge_to_faces = box_analysis.edge_to_faces;
    const auto& face_descriptors = box_analysis.face_descriptors;
    const auto& face_normals = box_analysis.face_normals;
    const auto& face_areas = box_analysis.face_areas;

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

    // =========================================================================
    // Part 3: L-Bracket Solid (Concave Re-entrant Corner)
    // =========================================================================
    std::cout << "\n--- Part 3: L-Bracket Solid (Concave Re-entrant Corner) ---\n";
    {
        fs::Selector block_sel("jot/Box");
        block_sel.parameters["width"] = 20.0;
        block_sel.parameters["height"] = 20.0;
        block_sel.parameters["depth"] = 10.0;
        block_sel.output = "$out";
        Processor::execute(&vfs, block_sel);
        Shape block_shape = vfs.read<Shape>(block_sel);

        fs::Selector notch_sel("jot/Box");
        notch_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
        notch_sel.parameters["height"] = nlohmann::json::array({0.0, 11.0});
        notch_sel.parameters["depth"] = nlohmann::json::array({-6.0, 6.0});
        notch_sel.output = "$out";
        Processor::execute(&vfs, notch_sel);
        Shape notch_shape = vfs.read<Shape>(notch_sel);

        fs::Selector cut_sel("jot/cut");
        cut_sel.parameters["$in"] = block_shape.to_json();
        cut_sel.parameters["tools"] = nlohmann::json::array({notch_shape.to_json()});
        cut_sel.output = "$out";
        Processor::execute(&vfs, cut_sel);
        Shape l_shape = vfs.read<Shape>(cut_sel);

        assert(l_shape.is_real() && l_shape.geometry.has_value());
        Geometry l_geo = vfs.read<Geometry>(*l_shape.geometry);
        ExactMesh l_mesh = boolean::Engine::geometry_to_mesh(l_geo);

        auto l_analysis = analyze_mesh(l_mesh);

        MoldParams params;
        params.draft = FT(0);
        params.padding = FT(5);
        params.policy = MoldDecompositionPolicy::analytical_envelope();

        auto result = decompose_mold_beam_search(
            l_mesh, l_analysis.edge_to_faces, params, /*max_pieces=*/4, /*candidates_per_level=*/4
        );

        std::cout << "  L-Bracket Decomposition: " << result.draw_dirs.size() << " pieces, complete: " 
                  << (result.is_complete ? "YES" : "NO") << "\n";
        for (size_t p = 0; p < result.draw_dirs.size(); ++p) {
            const auto& d = result.draw_dirs[p];
            std::cout << "    Piece #" << (p + 1) << ": (" 
                      << CGAL::to_double(d.x()) << ", " << CGAL::to_double(d.y()) << ", " << CGAL::to_double(d.z()) << ")\n";
        }
        assert(result.is_complete && "L-bracket decomposition must succeed completely");

        double total_l_area = 0.0;
        for (const auto& a : l_analysis.face_areas) total_l_area += CGAL::to_double(a);

        // Verify physical demoldability of each piece
        Tree l_tree(l_mesh.faces().begin(), l_mesh.faces().end(), l_mesh);
        double l_physical_contact = 0.0;
        for (size_t p = 0; p < result.solid_pieces.size(); ++p) {
            FT contact_area = FT(0);
            for (auto f : result.solid_pieces[p].faces()) {
                auto h = result.solid_pieces[p].halfedge(f);
                auto p0 = result.solid_pieces[p].point(result.solid_pieces[p].source(h));
                auto p1 = result.solid_pieces[p].point(result.solid_pieces[p].target(h));
                auto p2 = result.solid_pieces[p].point(result.solid_pieces[p].target(result.solid_pieces[p].next(h)));
                EK::Point_3 mid((p0.x() + p1.x() + p2.x()) / FT(3), (p0.y() + p1.y() + p2.y()) / FT(3), (p0.z() + p1.z() + p2.z()) / FT(3));
                if (l_tree.squared_distance(mid) < FT(1) / FT(10000)) {
                    contact_area += CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
                }
            }
            double piece_phys = CGAL::to_double(contact_area);
            l_physical_contact += piece_phys;

            MoldPiece mp{result.solid_pieces[p], result.draw_dirs[p], "piece", "#ffffff", (int)p};
            int backdraft_count = 0;
            FT backdraft_area = FT(0);
            bool is_demoldable = verify_piece_demoldability(mp, l_tree, params, &backdraft_count, &backdraft_area);
            std::cout << "    Piece #" << (p + 1) << " -> Contact: " << std::fixed << std::setprecision(1)
                      << piece_phys << " mm² | Demoldable: " << (is_demoldable ? "YES" : "NO")
                      << " (" << backdraft_count << " backdrafts)" << std::endl;
            assert(is_demoldable && backdraft_count == 0 && "L-bracket piece must demold with zero backdrafts");
        }
        std::cout << "  Total Physical Model Surface Covered: " << std::fixed << std::setprecision(1)
                  << l_physical_contact << " / " << total_l_area << " mm²" << std::endl;
        assert(l_physical_contact >= total_l_area - 0.1 && "Pieces must physically contact 100% of L-bracket model surface");
    }

    // =========================================================================
    // Part 4: C-Channel Solid (Overhang Occlusion & Fallback Verification)
    // =========================================================================
    std::cout << "\n--- Part 4: C-Channel Solid (Overhang Occlusion & Fallback Verification) ---\n";
    {
        fs::Selector box_sel("jot/Box");
        box_sel.parameters["width"] = 20.0;
        box_sel.parameters["height"] = 20.0;
        box_sel.parameters["depth"] = 20.0;
        box_sel.output = "$out";
        Processor::execute(&vfs, box_sel);
        Shape box_shape = vfs.read<Shape>(box_sel);

        // Horizontal slot cutting through Y: X in [0, 11], Y in [-11, 11], Z in [-3, 3]
        fs::Selector slot_sel("jot/Box");
        slot_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
        slot_sel.parameters["height"] = nlohmann::json::array({-11.0, 11.0});
        slot_sel.parameters["depth"] = nlohmann::json::array({-3.0, 3.0});
        slot_sel.output = "$out";
        Processor::execute(&vfs, slot_sel);
        Shape slot_shape = vfs.read<Shape>(slot_sel);

        fs::Selector cut_sel("jot/cut");
        cut_sel.parameters["$in"] = box_shape.to_json();
        cut_sel.parameters["tools"] = nlohmann::json::array({slot_shape.to_json()});
        cut_sel.output = "$out";
        Processor::execute(&vfs, cut_sel);
        Shape c_shape = vfs.read<Shape>(cut_sel);

        assert(c_shape.is_real() && c_shape.geometry.has_value());
        Geometry c_geo = vfs.read<Geometry>(*c_shape.geometry);
        ExactMesh c_mesh = boolean::Engine::geometry_to_mesh(c_geo);

        auto c_analysis = analyze_mesh(c_mesh);

        MoldParams params;
        params.draft = FT(0);
        params.padding = FT(5);
        params.policy = MoldDecompositionPolicy::analytical_envelope();

        auto result = decompose_mold_beam_search(
            c_mesh, c_analysis.edge_to_faces, params, /*max_pieces=*/6, /*candidates_per_level=*/4
        );

        std::cout << "  C-Channel Decomposition: " << result.draw_dirs.size() << " pieces, complete: " 
                  << (result.is_complete ? "YES" : "NO") << "\n";
        for (size_t p = 0; p < result.draw_dirs.size(); ++p) {
            const auto& d = result.draw_dirs[p];
            std::cout << "    Piece #" << (p + 1) << ": (" 
                      << CGAL::to_double(d.x()) << ", " << CGAL::to_double(d.y()) << ", " << CGAL::to_double(d.z()) << ")\n";
        }
        assert(result.is_complete && "C-channel decomposition must succeed completely");

        double total_c_area = 0.0;
        for (const auto& a : c_analysis.face_areas) total_c_area += CGAL::to_double(a);

        // Verify physical demoldability of each piece: zero backdrafts allowed
        Tree c_tree(c_mesh.faces().begin(), c_mesh.faces().end(), c_mesh);
        double c_physical_contact = 0.0;
        for (size_t p = 0; p < result.solid_pieces.size(); ++p) {
            FT contact_area = FT(0);
            for (auto f : result.solid_pieces[p].faces()) {
                auto h = result.solid_pieces[p].halfedge(f);
                auto p0 = result.solid_pieces[p].point(result.solid_pieces[p].source(h));
                auto p1 = result.solid_pieces[p].point(result.solid_pieces[p].target(h));
                auto p2 = result.solid_pieces[p].point(result.solid_pieces[p].target(result.solid_pieces[p].next(h)));
                EK::Point_3 mid((p0.x() + p1.x() + p2.x()) / FT(3), (p0.y() + p1.y() + p2.y()) / FT(3), (p0.z() + p1.z() + p2.z()) / FT(3));
                if (c_tree.squared_distance(mid) < FT(1) / FT(10000)) {
                    contact_area += CGAL::approximate_sqrt(CGAL::squared_area(p0, p1, p2));
                }
            }
            double piece_phys = CGAL::to_double(contact_area);
            c_physical_contact += piece_phys;

            MoldPiece mp{result.solid_pieces[p], result.draw_dirs[p], "piece", "#ffffff", (int)p};
            int backdraft_count = 0;
            FT backdraft_area = FT(0);
            bool is_demoldable = verify_piece_demoldability(mp, c_tree, params, &backdraft_count, &backdraft_area);
            std::cout << "    Piece #" << (p + 1) << " -> Contact: " << std::fixed << std::setprecision(1)
                      << piece_phys << " mm² | Demoldable: " << (is_demoldable ? "YES" : "NO")
                      << " (" << backdraft_count << " backdrafts)" << std::endl;
            assert(is_demoldable && backdraft_count == 0 && "C-channel piece must demold with zero backdrafts");
        }
        std::cout << "  Total Physical Model Surface Covered: " << std::fixed << std::setprecision(1)
                  << c_physical_contact << " / " << total_c_area << " mm²" << std::endl;
        assert(c_physical_contact >= total_c_area - 0.1 && "Pieces must physically contact 100% of C-channel model surface");

        // Verify that terminal demoldability fallback executed at least once on occluded faces
        std::cout << "  C-Channel Fallback Count: " << result.terminal_fallbacks << std::endl;
        assert(result.terminal_fallbacks >= 1 && "C-channel must trigger terminal fallback on occluded faces");
    }

    std::cout << "\n========================================================\n"
              << "Mold Policy Test Suite Completed Successfully!\n"
              << "========================================================" << std::endl;
    return 0;
}
