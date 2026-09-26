#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/scoring.h"
#include "boolean/engine.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_scoring_test");
    register_all_ops(&vfs);

    std::cout << "Testing candidate scoring function (mold/scoring.h)..." << std::endl;

    // 1. Create a 3D Box (20 x 20 x 10)
    // Top & Bottom faces: 20x20 = 400 mm^2 each.
    // Four side faces: 20x10 = 200 mm^2 each.
    // Total surface area = 2*400 + 4*200 = 1600 mm^2.
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

    // Prepare mesh face properties
    std::vector<ExactMesh::Face_index> face_descriptors;
    std::vector<EK::Vector_3> face_normals(mesh.num_faces());
    std::vector<FT> face_areas(mesh.num_faces());
    boolean::ExactMesh::Property_map<boolean::ExactMesh::Face_index, bool> is_handled = mesh.add_property_map<ExactMesh::Face_index, bool>("f:is_handled", false).first;

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
    }

    // 2. Score candidate directions with draft threshold > 0 (strictly positive draft faces)
    FT min_dot = FT(1) / FT(100); // 0.01, excludes 0 draft side walls

    EK::Vector_3 dir_top(FT(0), FT(0), FT(1));
    EK::Vector_3 dir_bottom(FT(0), FT(0), FT(-1));
    EK::Vector_3 dir_side(FT(1), FT(0), FT(0));

    CandidateScore score_top = score_candidate_direction(
        dir_top, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );
    CandidateScore score_bottom = score_candidate_direction(
        dir_bottom, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );
    CandidateScore score_side = score_candidate_direction(
        dir_side, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );

    std::cout << "  - Score Top (+Z): responsible_area=" << CGAL::to_double(score_top.responsible_area) << " mm^2" << std::endl;
    std::cout << "  - Score Bottom (-Z): responsible_area=" << CGAL::to_double(score_bottom.responsible_area) << " mm^2" << std::endl;
    std::cout << "  - Score Side (+X): responsible_area=" << CGAL::to_double(score_side.responsible_area) << " mm^2" << std::endl;

    // Top face is 400 mm^2 (composed of 2 triangles of 200 mm^2 each)
    assert(std::abs(CGAL::to_double(score_top.responsible_area) - 400.0) < 1e-3);
    // Bottom face is 400 mm^2
    assert(std::abs(CGAL::to_double(score_bottom.responsible_area) - 400.0) < 1e-3);
    // Side face is 200 mm^2
    assert(std::abs(CGAL::to_double(score_side.responsible_area) - 200.0) < 1e-3);

    // Verify ranking comparator: Top (400 mm^2) beats Side (200 mm^2)
    assert(score_top > score_side);
    assert(score_bottom > score_side);
    assert(!(score_side > score_top));

    // 3. Mark Top faces as handled (simulating Piece 1 extracting along +Z)
    for (auto f : face_descriptors) {
        if (face_normals[f.idx()] * dir_top >= min_dot) {
            is_handled[f] = true;
        }
    }

    // 4. Re-score candidate directions after Piece 1 extraction
    CandidateScore score_top_post = score_candidate_direction(
        dir_top, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );
    CandidateScore score_bottom_post = score_candidate_direction(
        dir_bottom, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );
    CandidateScore score_side_post = score_candidate_direction(
        dir_side, face_descriptors, face_normals, face_areas, is_handled, min_dot
    );

    std::cout << "  - Post-Piece 1 Score Top (+Z): responsible_area=" << CGAL::to_double(score_top_post.responsible_area) << " mm^2" << std::endl;
    std::cout << "  - Post-Piece 1 Score Bottom (-Z): responsible_area=" << CGAL::to_double(score_bottom_post.responsible_area) << " mm^2" << std::endl;
    std::cout << "  - Post-Piece 1 Score Side (+X): responsible_area=" << CGAL::to_double(score_side_post.responsible_area) << " mm^2" << std::endl;

    // Top face is already handled, so its responsible area drops to 0
    assert(score_top_post.responsible_area == FT(0));
    // Bottom face is still unhandled (400 mm^2)
    assert(std::abs(CGAL::to_double(score_bottom_post.responsible_area) - 400.0) < 1e-3);
    // Side face is still unhandled (200 mm^2)
    assert(std::abs(CGAL::to_double(score_side_post.responsible_area) - 200.0) < 1e-3);

    // Bottom now dominates both Side and Top
    assert(score_bottom_post > score_side_post);
    assert(score_bottom_post > score_top_post);
    assert(score_side_post > score_top_post);

    std::cout << "✅ ALL Candidate Scoring Tests Passed" << std::endl;
    return 0;
}
