#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/beam_search.h"
#include "boolean/engine.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_beam_search_test");
    register_all_ops(&vfs);

    std::cout << "Testing Energy-Minimizing Beam Search (mold/beam_search.h)..." << std::endl;

    // =========================================================================
    // Test 1: Simple 3D Box (Certified 2-Piece Assembly along +/- Z)
    // =========================================================================
    {
        std::cout << "  - Test 1: Simple 3D Box (20x20x10) Certified 2-Piece Assembly..." << std::endl;

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
        for (auto f : mesh.faces()) {
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        MoldParams params;
        params.draft = FT(0);
        params.padding = FT(5);

        auto result = decompose_mold_beam_search(
            mesh, edge_to_faces, params, /*max_pieces=*/4, /*candidates_per_level=*/4
        );

        assert(result.is_complete && "Beam search MUST complete 100% surface decomposition of a simple box!");
        assert(result.draw_dirs.size() == 2 && "Box decomposition MUST find the minimal 2-piece assembly!");
        assert(result.solid_wedges.size() == 2 && "Must extract exactly 2 solid wedges!");
        assert(result.remaining_unhandled_area == FT(0) && "Remaining unhandled area must be exactly 0!");

        // Verify antipodal pull directions (+Z and -Z)
        const auto& d1 = result.draw_dirs[0];
        const auto& d2 = result.draw_dirs[1];
        FT dot_opp = d1 * (-d2);
        FT len_sq1 = d1.squared_length();
        FT len_sq2 = d2.squared_length();
        assert(dot_opp * dot_opp == len_sq1 * len_sq2 && "Piece 1 and Piece 2 draw directions must be strictly antipodal!");

        std::cout << "    [Passed] 2-Piece certified decomposition: Piece 1 dir=("
                  << CGAL::to_double(d1.x()) << ", " << CGAL::to_double(d1.y()) << ", " << CGAL::to_double(d1.z())
                  << "), Piece 2 dir=("
                  << CGAL::to_double(d2.x()) << ", " << CGAL::to_double(d2.y()) << ", " << CGAL::to_double(d2.z())
                  << "), final energy = " << CGAL::to_double(result.final_energy) << std::endl;
    }

    // =========================================================================
    // Test 2: Concave L-Bracket (Wall Cross Product Ingress & Complete Coverage)
    // =========================================================================
    {
        std::cout << "  - Test 2: Concave L-Bracket (Analytical Ingress & Coverage)..." << std::endl;

        fs::Selector l_box_sel("jot/Box");
        l_box_sel.parameters["width"] = 20.0;
        l_box_sel.parameters["height"] = 20.0;
        l_box_sel.parameters["depth"] = 10.0;
        Shape l_base = vfs.read<Shape>(l_box_sel.with_output("$out"));

        fs::Selector l_notch_sel("jot/Box");
        l_notch_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
        l_notch_sel.parameters["height"] = nlohmann::json::array({0.0, 11.0});
        l_notch_sel.parameters["depth"] = nlohmann::json::array({-6.0, 6.0});
        Shape l_notch = vfs.read<Shape>(l_notch_sel.with_output("$out"));

        fs::Selector l_cut_sel("jot/cut");
        l_cut_sel.parameters["$in"] = l_base.to_json();
        l_cut_sel.parameters["tools"] = nlohmann::json::array({l_notch.to_json()});
        Shape l_shape = vfs.read<Shape>(l_cut_sel.with_output("$out"));

        Geometry l_geo = vfs.read<Geometry>(*l_shape.geometry);
        ExactMesh mesh = boolean::Engine::geometry_to_mesh(l_geo);

        std::map<EdgeKey, std::vector<int>> edge_to_faces;
        for (auto f : mesh.faces()) {
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        MoldParams params;
        params.draft = FT(0);
        params.padding = FT(5);

        auto result = decompose_mold_beam_search(
            mesh, edge_to_faces, params, /*max_pieces=*/4, /*candidates_per_level=*/4
        );

        assert(result.is_complete && "Beam search MUST complete 100% surface decomposition of L-bracket!");
        assert(result.draw_dirs.size() >= 2 && "Must find a complete decomposition with at least 2 pieces!");
        assert(result.remaining_unhandled_area == FT(0) && "Remaining unhandled area must be 0!");

        std::cout << "    [Passed] Concave L-bracket decomposition complete with " 
                  << result.draw_dirs.size() << " pieces, remaining area = "
                  << CGAL::to_double(result.remaining_unhandled_area) << " mm^2." << std::endl;
    }

    std::cout << "\nALL MOLD BEAM SEARCH TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
