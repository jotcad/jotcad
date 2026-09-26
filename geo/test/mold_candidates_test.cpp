#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/candidates.h"
#include "mold/scoring.h"
#include "boolean/engine.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::mold;

int main() {
    MockVFS vfs("mold_candidates_test");
    register_all_ops(&vfs);

    std::cout << "Testing candidate generation (mold/candidates.h)..." << std::endl;

    // 1. Create a Concave L-Bracket (20x20x10 base with 11x11x12 notch cut)
    // The notch creates two perpendicular vertical walls:
    // Wall 1 with normal (1, 0, 0), Wall 2 with normal (0, 1, 0).
    // Their analytical cross product (n1 x n2) is (0, 0, 1), parallel to the notch corner!
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
    boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(l_geo);
    assert(mesh.number_of_faces() > 0);

    std::vector<boolean::ExactMesh::Face_index> face_descriptors;
    std::vector<EK::Vector_3> face_normals(mesh.num_faces());
    std::vector<FT> face_areas(mesh.num_faces());
    boolean::ExactMesh::Property_map<boolean::ExactMesh::Face_index, bool> is_handled =
        mesh.add_property_map<boolean::ExactMesh::Face_index, bool>("f:is_handled", false).first;

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

    // 2. Generate candidate directions
    std::vector<EK::Vector_3> prior_dirs = { EK::Vector_3(FT(0), FT(0), FT(1)) };
    auto candidates = generate_candidate_directions(
        mesh, face_descriptors, face_normals, face_areas, is_handled, prior_dirs, 16
    );

    std::cout << "  - Generated " << candidates.size() << " unique candidate directions." << std::endl;
    assert(candidates.size() >= 6);

    // 3. Verify Antipodal Candidate Inclusion: -prior_dirs[0] = (0, 0, -1)
    bool has_antipodal = false;
    for (const auto& d : candidates) {
        if (d.x() == FT(0) && d.y() == FT(0) && d.z() < FT(0)) {
            has_antipodal = true;
            break;
        }
    }
    assert(has_antipodal && "Must contain antipodal vector (0, 0, -1) from prior draw direction!");
    std::cout << "  - Confirmed antipodal candidate (0, 0, -1) present." << std::endl;

    // 4. Verify Analytical Cross Product Inclusion: (0, 0, 1) parallel to notch corner
    bool has_z_axis = false;
    for (const auto& d : candidates) {
        if (d.x() == FT(0) && d.y() == FT(0) && d.z() > FT(0)) {
            has_z_axis = true;
            break;
        }
    }
    assert(has_z_axis && "Must contain vertical corner axis (0, 0, 1) from wall cross products!");
    std::cout << "  - Confirmed analytical wall cross-product axis (0, 0, 1) present." << std::endl;

    // 5. Verify Exact Deduplication: No two candidates are parallel in the same direction
    for (size_t i = 0; i < candidates.size(); ++i) {
        for (size_t j = i + 1; j < candidates.size(); ++j) {
            EK::Vector_3 cp = CGAL::cross_product(candidates[i], candidates[j]);
            FT dot = candidates[i] * candidates[j];
            assert(!(cp.squared_length() == FT(0) && dot > FT(0)) && "Candidates list must be strictly deduplicated!");
        }
    }
    std::cout << "  - Confirmed all candidates are strictly deduplicated." << std::endl;

    // 6. Score all generated candidates using scoring.h
    std::vector<std::pair<::jotcad::geo::mold::CandidateScore, EK::Vector_3>> scored_cands;
    for (const auto& d : candidates) {
        ::jotcad::geo::mold::CandidateScore s = ::jotcad::geo::mold::score_candidate_direction(d, face_descriptors, face_normals, face_areas, is_handled, FT(0));
        scored_cands.push_back({s, d});
    }

    std::sort(scored_cands.begin(), scored_cands.end(), [](const auto& a, const auto& b) {
        return a.first > b.first;
    });

    std::cout << "  - Top candidate direction: ("
              << CGAL::to_double(scored_cands[0].second.x()) << ", "
              << CGAL::to_double(scored_cands[0].second.y()) << ", "
              << CGAL::to_double(scored_cands[0].second.z()) << ") responsible for "
              << CGAL::to_double(scored_cands[0].first.responsible_area) << " mm^2" << std::endl;

    assert(scored_cands[0].first.responsible_area > FT(100.0) && "Top candidate must take responsibility for substantial surface area!");

    std::cout << "✅ ALL Candidate Generation Tests Passed" << std::endl;
    return 0;
}
