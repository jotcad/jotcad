#include <iostream>
#include <cassert>
#include "protocols.h"
#include "vfs_node.h"
#include "processor.h"
#include "data/shape.h"
#include "ops/box_op.h"
#include "ops/orb_op.h"
#include "ops/cut_op.h"
#include "ops/stamp_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config;
    config.id = "stamp-test";
    config.storage_dir = ".vfs_storage_stamp_test";
    fs::VFSNode vfs(config);

    // 1. Create a flat Box (20x20)
    fs::Selector box_sel("jot/Box", {{"width", 20.0}, {"height", 20.0}, {"depth", 0.0}});
    BoxOp<>::execute(&vfs, box_sel, {0.0, 20.0}, {0.0, 20.0}, {0.0, 0.0});
    Shape box = vfs.read<Shape>(box_sel.with_output("$out"));

    // 2. Create an Orb (diameter 15)
    fs::Selector orb_sel("jot/Orb", {{"diameter", 15.0}});
    OrbOp<>::execute(&vfs, orb_sel, {-7.5, 7.5}, {0.0, 0.0}, {0.0, 0.0}, {0.0, 0.0}, 0.1);
    Shape orb = vfs.read<Shape>(orb_sel.with_output("$out"));
    orb.tf = Matrix::translate(FT(10), FT(10), FT(0));

    // 3. Test CUT (should be flat)
    fs::Selector cut_sel("jot/cut");
    CutOp<>::execute(&vfs, cut_sel, box, {orb}, false);
    Shape cut_res = vfs.read<Shape>(cut_sel.with_output("$out"));

    Geometry cut_geo = vfs.read<Geometry>(*cut_res.geometry);
    std::cout << "Cut Result Vertices: " << cut_geo.vertices.size() << std::endl;
    
    bool is_flat = true;
    for (const auto& v : cut_geo.vertices) {
        if (std::abs(CGAL::to_double(v.z)) > 1e-6) {
            is_flat = false;
            break;
        }
    }
    std::cout << "Is Cut Flat? " << (is_flat ? "YES" : "NO") << std::endl;
    assert(is_flat);

    // 4. Test STAMP (should have membrane effect / not flat)
    fs::Selector stamp_sel("jot/stamp");
    StampOp<>::execute(&vfs, stamp_sel, box, {orb});
    Shape stamp_res = vfs.read<Shape>(stamp_sel.with_output("$out"));

    Geometry stamp_geo = vfs.read<Geometry>(*stamp_res.geometry);
    std::cout << "Stamp Result Vertices: " << stamp_geo.vertices.size() << std::endl;

    bool has_3d = false;
    for (const auto& v : stamp_geo.vertices) {
        if (std::abs(CGAL::to_double(v.z)) > 1.0) { // Should have significant Z depth from the Orb
            has_3d = true;
            break;
        }
    }
    std::cout << "Has Stamp 3D depth? " << (has_3d ? "YES" : "NO") << std::endl;
    assert(has_3d);

    std::cout << "Stamp and Cut tests passed!" << std::endl;
    return 0;
}
