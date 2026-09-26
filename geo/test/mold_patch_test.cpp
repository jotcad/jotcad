#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/patch.h"
#include "boolean/engine.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_patch_test");
    register_all_ops(&vfs);

    std::cout << "Testing visible patch & disjoint component extraction (mold/patch.h)..." << std::endl;

    // =========================================================================
    // Test 1: Single Continuous Disk Patch (Box 20 x 20 x 10)
    // =========================================================================
    {
        std::cout << "  - Test 1: Single Box Top Face (Continuous Disk, cycle_count = 1)..." << std::endl;

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
        assert(mesh.number_of_faces() > 0);

        std::vector<ExactMesh::Face_index> face_descriptors;
        std::vector<EK::Vector_3> face_normals(mesh.num_faces());
        std::vector<FT> face_areas(mesh.num_faces());
        std::map<EdgeKey, std::vector<int>> edge_to_faces;
        FaceBoolMap is_handled = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

        for (auto f : mesh.faces()) {
            face_descriptors.push_back(f);
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            auto p0 = mesh.point(mesh.source(h));
            auto p1 = mesh.point(mesh.target(h));
            auto p2 = mesh.point(mesh.target(mesh.next(h)));
            EK::Vector_3 raw_n = CGAL::normal(p0, p1, p2);
            double len = std::sqrt(CGAL::to_double(raw_n.squared_length()));
            if (len > 1e-9) {
                face_normals[idx] = EK::Vector_3(
                    FT(CGAL::to_double(raw_n.x()) / len),
                    FT(CGAL::to_double(raw_n.y()) / len),
                    FT(CGAL::to_double(raw_n.z()) / len)
                );
            } else {
                face_normals[idx] = raw_n;
            }
            face_areas[idx] = std::sqrt(CGAL::to_double(CGAL::squared_area(p0, p1, p2)));

            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        // Extract candidate patch along +Z with strict positive draft min_dot = 0.5
        EK::Vector_3 d(FT(0), FT(0), FT(1));
        CandidatePatch patch = extract_candidate_patch(
            mesh, face_descriptors, face_normals, face_areas, edge_to_faces, is_handled, d, FT(1)/FT(2)
        );

        assert(patch.is_valid && "Top face of box must be a valid topological disk!");
        assert(patch.cycle_count == 1 && "Top face must have exactly 1 boundary cycle!");
        assert(patch.faces.size() == 2 && "Top face of triangulated box has 2 triangles!");
        assert(CGAL::to_double(patch.total_area) > 399.9 && CGAL::to_double(patch.total_area) < 400.1);
        std::cout << "    [Passed] Extracted " << patch.faces.size() << " faces, area = " 
                  << CGAL::to_double(patch.total_area) << " mm^2, cycles = " << patch.cycle_count << std::endl;
    }

    // =========================================================================
    // Test 2: Disjoint Island Aggregation (Two Disconnected Boxes along +Z)
    // =========================================================================
    {
        std::cout << "  - Test 2: Disjoint Island Aggregation (2 separate boxes along +Z)..." << std::endl;

        // Box A at [-25, -15] of size 10x10x10
        // Box B at [+15, +25] of size 10x10x10
        fs::Selector box_a("jot/Box");
        box_a.parameters["width"] = nlohmann::json::array({-25.0, -15.0});
        box_a.parameters["height"] = 10.0;
        box_a.parameters["depth"] = 10.0;
        Shape s_a = vfs.read<Shape>(box_a.with_output("$out"));

        fs::Selector box_b("jot/Box");
        box_b.parameters["width"] = nlohmann::json::array({15.0, 25.0});
        box_b.parameters["height"] = 10.0;
        box_b.parameters["depth"] = 10.0;
        Shape s_b = vfs.read<Shape>(box_b.with_output("$out"));

        fs::Selector fuse_sel("jot/fuse");
        fuse_sel.parameters["$in"] = s_a.to_json();
        fuse_sel.parameters["tools"] = nlohmann::json::array({s_b.to_json()});
        Shape fused_shape = vfs.read<Shape>(fuse_sel.with_output("$out"));
        assert(fused_shape.is_real() && fused_shape.geometry.has_value());

        Geometry fused_geo = vfs.read<Geometry>(*fused_shape.geometry);
        ExactMesh mesh = boolean::Engine::geometry_to_mesh(fused_geo);

        std::vector<ExactMesh::Face_index> face_descriptors;
        std::vector<EK::Vector_3> face_normals(mesh.num_faces());
        std::vector<FT> face_areas(mesh.num_faces());
        std::map<EdgeKey, std::vector<int>> edge_to_faces;
        FaceBoolMap is_handled = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

        for (auto f : mesh.faces()) {
            face_descriptors.push_back(f);
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            auto p0 = mesh.point(mesh.source(h));
            auto p1 = mesh.point(mesh.target(h));
            auto p2 = mesh.point(mesh.target(mesh.next(h)));
            EK::Vector_3 raw_n = CGAL::normal(p0, p1, p2);
            double len = std::sqrt(CGAL::to_double(raw_n.squared_length()));
            if (len > 1e-9) {
                face_normals[idx] = EK::Vector_3(
                    FT(CGAL::to_double(raw_n.x()) / len),
                    FT(CGAL::to_double(raw_n.y()) / len),
                    FT(CGAL::to_double(raw_n.z()) / len)
                );
            } else {
                face_normals[idx] = raw_n;
            }
            face_areas[idx] = std::sqrt(CGAL::to_double(CGAL::squared_area(p0, p1, p2)));

            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        // Along +Z, both Box A and Box B have top faces of 10x10 = 100 mm^2 each.
        // Total aggregated area must equal 200 mm^2.
        EK::Vector_3 d(FT(0), FT(0), FT(1));
        CandidatePatch patch = extract_candidate_patch(
            mesh, face_descriptors, face_normals, face_areas, edge_to_faces, is_handled, d, FT(1)/FT(2)
        );

        assert(patch.is_valid && "Disjoint non-overlapping islands must form a valid aggregated patch!");
        assert(patch.faces.size() == 4 && "2 boxes * 2 triangles each = 4 triangles total!");
        assert(CGAL::to_double(patch.total_area) > 199.9 && CGAL::to_double(patch.total_area) < 200.1);
        std::cout << "    [Passed] Aggregated " << patch.faces.size() << " faces across 2 disjoint islands, total area = "
                  << CGAL::to_double(patch.total_area) << " mm^2." << std::endl;
    }

    // =========================================================================
    // Test 3: Annulus / Through-Hole Detection (cycle_count > 1 => is_valid = false)
    // =========================================================================
    {
        std::cout << "  - Test 3: Hole Detection on Annular Face (cycle_count > 1)..." << std::endl;

        // Outer box 30 x 30 x 10, subtract inner box 10 x 10 x 20
        fs::Selector outer_sel("jot/Box");
        outer_sel.parameters["width"] = 30.0;
        outer_sel.parameters["height"] = 30.0;
        outer_sel.parameters["depth"] = 10.0;
        Shape outer = vfs.read<Shape>(outer_sel.with_output("$out"));

        fs::Selector inner_sel("jot/Box");
        inner_sel.parameters["width"] = 10.0;
        inner_sel.parameters["height"] = 10.0;
        inner_sel.parameters["depth"] = 20.0;
        Shape inner = vfs.read<Shape>(inner_sel.with_output("$out"));

        fs::Selector cut_sel("jot/cut");
        cut_sel.parameters["$in"] = outer.to_json();
        cut_sel.parameters["tools"] = nlohmann::json::array({inner.to_json()});
        Shape hollow_shape = vfs.read<Shape>(cut_sel.with_output("$out"));
        assert(hollow_shape.is_real() && hollow_shape.geometry.has_value());

        Geometry hollow_geo = vfs.read<Geometry>(*hollow_shape.geometry);
        ExactMesh mesh = boolean::Engine::geometry_to_mesh(hollow_geo);

        std::vector<ExactMesh::Face_index> face_descriptors;
        std::vector<EK::Vector_3> face_normals(mesh.num_faces());
        std::vector<FT> face_areas(mesh.num_faces());
        std::map<EdgeKey, std::vector<int>> edge_to_faces;
        FaceBoolMap is_handled = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

        for (auto f : mesh.faces()) {
            face_descriptors.push_back(f);
            size_t idx = f.idx();
            auto h = mesh.halfedge(f);
            auto p0 = mesh.point(mesh.source(h));
            auto p1 = mesh.point(mesh.target(h));
            auto p2 = mesh.point(mesh.target(mesh.next(h)));
            EK::Vector_3 raw_n = CGAL::normal(p0, p1, p2);
            double len = std::sqrt(CGAL::to_double(raw_n.squared_length()));
            if (len > 1e-9) {
                face_normals[idx] = EK::Vector_3(
                    FT(CGAL::to_double(raw_n.x()) / len),
                    FT(CGAL::to_double(raw_n.y()) / len),
                    FT(CGAL::to_double(raw_n.z()) / len)
                );
            } else {
                face_normals[idx] = raw_n;
            }
            face_areas[idx] = std::sqrt(CGAL::to_double(CGAL::squared_area(p0, p1, p2)));

            for (int i = 0; i < 3; ++i) {
                int u = (int)mesh.source(h);
                int v = (int)mesh.target(h);
                if (u > v) std::swap(u, v);
                edge_to_faces[{u, v}].push_back((int)idx);
                h = mesh.next(h);
            }
        }

        // Top face of hollow box has an outer boundary and an inner hole boundary (cycle_count = 2)
        EK::Vector_3 d(FT(0), FT(0), FT(1));
        CandidatePatch patch = extract_candidate_patch(
            mesh, face_descriptors, face_normals, face_areas, edge_to_faces, is_handled, d, FT(1)/FT(2)
        );

        assert(!patch.is_valid && "Annular face with internal hole must be rejected as invalid disk (cycle_count > 1)!");
        assert(patch.cycle_count == 2 && "Annular top face must have exactly 2 boundary cycles (outer + hole)!");
        std::cout << "    [Passed] Correctly flagged annular face as non-disk (cycles = " << patch.cycle_count 
                  << ", is_valid = " << (patch.is_valid ? "true" : "false") << ")." << std::endl;
    }

    std::cout << "\nALL MOLD PATCH TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
