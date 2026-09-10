#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "infra/stl.h"
#include "mold/repair.h"

using namespace jotcad;
using namespace jotcad::geo;

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
    assert(moving_pieces >= 2);

    std::cout << "  ✅ Box mold test passed." << std::endl;
    return 0;
}
