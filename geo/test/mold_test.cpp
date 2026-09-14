#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "infra/stl.h"
#include "mold/repair.h"
#include "mold/tide.h"
#include "boolean/corefine.h"

using namespace ::jotcad;
using namespace ::jotcad::geo;
using namespace ::jotcad::geo::mold;
using namespace ::fs;
using ExactMesh = ::jotcad::geo::mold::ExactMesh;
using ::CGAL::to_double;

int main() {
    MockVFS vfs("mold_test");
    register_all_ops(&vfs);

    std::cout << "Testing jot/mold operator..." << std::endl;

    // 1. Test 2-piece mold on convex box
    std::cout << "  - Testing 2-piece mold on Box..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    Shape box_shape = vfs.read<Shape>(box_sel.with_output("$out"));

    fs::Selector mold_sel("jot/mold");
    mold_sel.parameters["$in"] = box_shape.to_json();
    mold_sel.parameters["padding"] = 5.0;
    mold_sel.parameters["draft"] = 0.0;
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape box_mold = vfs.read<Shape>(mold_sel);

    assert(box_mold.components.size() >= 2); // mold pieces
    std::cout << "    - Total box mold components: " << box_mold.components.size() << std::endl;
    int moving_pieces = 0;
    int stationary_pieces = 0;
    for (const auto& s : box_mold) {
        if (s.has_tag("mold/role", "piece")) {
            int piece_id = s.tags["mold/piece"].get<int>();
            double vol = 0.0;
            if (s.geometry.has_value()) {
                Geometry g = vfs.read<Geometry>(*s.geometry);
                mold::ExactMesh m = boolean::Engine::geometry_to_mesh(g);
                vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(m));
            }
            if (s.has_tag("mold/pull_vector")) {
                moving_pieces++;
                std::cout << "      Moving Piece #" << piece_id
                          << " pull_vector=" << s.tags["mold/pull_vector"]
                          << " volume=" << vol << " mm^3" << std::endl;
            } else {
                stationary_pieces++;
                std::cout << "      Stationary Dead Region #" << piece_id
                          << " volume=" << vol << " mm^3" << std::endl;
            }
        }
    }
    assert(moving_pieces == 2);
    assert(stationary_pieces == 0);

    // 2. Direct Rising Tide Wedge Test on Box
    std::cout << "  - Testing construct_rising_tide_wedge on Box..." << std::endl;
    Geometry box_geo = vfs.read<Geometry>(*box_shape.geometry);
    mold::ExactMesh box_mesh = boolean::Engine::geometry_to_mesh(box_geo);

    std::vector<mold::ExactMesh::Face_index> top_faces;
    std::vector<mold::ExactMesh::Face_index> bottom_faces;
    for (auto f : box_mesh.faces()) {
        auto h = box_mesh.halfedge(f);
        auto p0 = box_mesh.point(box_mesh.source(h));
        auto p1 = box_mesh.point(box_mesh.target(h));
        auto p2 = box_mesh.point(box_mesh.target(box_mesh.next(h)));
        auto n = CGAL::normal(p0, p1, p2);
        if (n.z() > FT(0)) top_faces.push_back(f);
        if (n.z() < FT(0)) bottom_faces.push_back(f);
    }
    assert(top_faces.size() == 2);
    assert(bottom_faces.size() == 2);

    auto top_wedge_res = mold::construct_rising_tide_wedge(box_mesh, top_faces, EK::Vector_3(0, 0, 1), FT(10), FT(0));
    auto bot_wedge_res = mold::construct_rising_tide_wedge(box_mesh, bottom_faces, EK::Vector_3(0, 0, -1), FT(10), FT(0));

    double top_wedge_vol = ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(top_wedge_res.solid_wedge));
    double bot_wedge_vol = ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(bot_wedge_res.solid_wedge));
    std::cout << "    - Top Rising Tide Wedge volume: " << top_wedge_vol << " mm^3" << std::endl;
    std::cout << "    - Bottom Rising Tide Wedge volume: " << bot_wedge_vol << " mm^3" << std::endl;
    assert(std::abs(top_wedge_vol - 3500.0) < 1e-4);
    assert(std::abs(bot_wedge_vol - 3500.0) < 1e-4);

    mold::ExactMesh overlap_mesh;
    ::jotcad::geo::boolean::corefine_intersection(top_wedge_res.solid_wedge, bot_wedge_res.solid_wedge, overlap_mesh, fix::KissMode::WELD, FT(0.01), "top ∩ bot");
    double overlap_vol = overlap_mesh.is_empty() ? 0.0 : ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(overlap_mesh));
    std::cout << "    - Overlap volume between top and bottom wedges: " << overlap_vol << " mm^3" << std::endl;
    assert(overlap_vol < 1e-4);

    mold::ExactMesh diff_mesh;
    bool ok_diff = ::jotcad::geo::boolean::corefine_difference(top_wedge_res.solid_wedge, box_mesh, diff_mesh, fix::KissMode::WELD, FT(0.01), "top_wedge \\ box");
    double diff_vol = diff_mesh.is_empty() ? 0.0 : ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(diff_mesh));
    std::cout << "    - Volume after corefine_difference(top_wedge \\ box): " << diff_vol << " mm^3 (ok=" << ok_diff << ")" << std::endl;
    assert(ok_diff);
    assert(std::abs(diff_vol - 3500.0) < 1e-4);

    std::cout << "  ✅ Box mold test passed." << std::endl;
    return 0;
}
