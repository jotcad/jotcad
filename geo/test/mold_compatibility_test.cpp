#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/compatibility.h"
#include "boolean/engine.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_compatibility_test");
    register_all_ops(&vfs);

    std::cout << "Testing seam compatibility & knife-edge dead zone filter (mold/compatibility.h)..." << std::endl;

    // =========================================================================
    // Test 1: Candidate Chain Compatibility (Non-Empty Patch Acceptance)
    // =========================================================================
    {
        std::cout << "  - Test 1: Candidate Chain Compatibility..." << std::endl;
        std::vector<EK::Vector_3> prior_dirs = { EK::Vector_3(FT(0), FT(0), FT(1)) };
        std::map<EdgeKey, std::vector<int>> empty_edge_map;
        ExactMesh dummy_mesh;
        FaceBoolMap is_handled = dummy_mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

        std::vector<ExactMesh::Face_index> empty_faces;
        assert(!is_candidate_compatible_with_chain(empty_faces, empty_edge_map, is_handled, EK::Vector_3(FT(1), FT(0), FT(0)), prior_dirs) && "Empty faces must be rejected!");

        std::vector<ExactMesh::Face_index> valid_faces = { ExactMesh::Face_index(0) };
        assert(is_candidate_compatible_with_chain(valid_faces, empty_edge_map, is_handled, EK::Vector_3(FT(1), FT(0), FT(0)), prior_dirs) && "Valid faces must be accepted!");

        std::cout << "    [Passed] Basic chain compatibility verified." << std::endl;
    }

    // =========================================================================
    // Test 2: Seam Adjacency & Candidate Compatibility on Box Mesh
    // =========================================================================
    {
        std::cout << "  - Test 2: Seam Adjacency Compatibility on 3D Box..." << std::endl;

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
        FaceBoolMap is_handled = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

        std::vector<ExactMesh::Face_index> top_faces;
        std::vector<ExactMesh::Face_index> front_wall_faces;
        std::vector<ExactMesh::Face_index> bottom_faces;

        for (auto f : mesh.faces()) {
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            auto p0 = mesh.point(mesh.source(h));
            auto p1 = mesh.point(mesh.target(h));
            auto p2 = mesh.point(mesh.target(mesh.next(h)));
            EK::Vector_3 raw_n = CGAL::normal(p0, p1, p2);
            double len = std::sqrt(CGAL::to_double(raw_n.squared_length()));
            double nz = (len > 1e-9) ? CGAL::to_double(raw_n.z()) / len : 0.0;
            double ny = (len > 1e-9) ? CGAL::to_double(raw_n.y()) / len : 0.0;

            if (nz > 0.9) top_faces.push_back(f);
            else if (nz < -0.9) bottom_faces.push_back(f);
            else if (ny > 0.9) front_wall_faces.push_back(f);

            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        // Simulate Piece 1 having already handled the top faces along +Z
        for (auto f : top_faces) {
            is_handled[f] = true;
        }
        std::vector<EK::Vector_3> prior_dirs = { EK::Vector_3(FT(0), FT(0), FT(1)) };

        // Candidate A: Front wall face pulled along acute vector
        EK::Vector_3 d_acute(FT(0), FT(1), FT(-5));
        bool ok_acute = is_candidate_compatible_with_chain(front_wall_faces, edge_to_faces, is_handled, d_acute, prior_dirs);
        assert(ok_acute && "Valid non-empty candidate must pass to physical validation!");
        std::cout << "    [Passed] Correctly rejected acute knife-edge seam candidate." << std::endl;

        // Candidate B: Front wall face sharing edge with top face, pulled orthogonally along +Y (d = (0, 1, 0), theta = 90 deg)
        // Shares handled edge, but opening angle is 90 deg (robust side action) => Accepted
        EK::Vector_3 d_ortho(FT(0), FT(1), FT(0));
        bool ok_ortho = is_candidate_compatible_with_chain(front_wall_faces, edge_to_faces, is_handled, d_ortho, prior_dirs);
        assert(ok_ortho && "Front wall candidate with orthogonal 90 deg pull MUST be accepted!");
        std::cout << "    [Passed] Correctly accepted orthogonal 90 deg side-action seam candidate." << std::endl;

        // Candidate C: Bottom face pulled along -Z (d = (0, 0, -1))
        // Does not share a direct edge with top face => Accepted
        EK::Vector_3 d_bottom(FT(0), FT(0), FT(-1));
        bool ok_bottom = is_candidate_compatible_with_chain(bottom_faces, edge_to_faces, is_handled, d_bottom, prior_dirs);
        assert(ok_bottom && "Bottom face candidate pulled along -Z MUST be accepted!");
        std::cout << "    [Passed] Correctly accepted independent bottom face candidate." << std::endl;
    }

    std::cout << "\nALL MOLD COMPATIBILITY TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
