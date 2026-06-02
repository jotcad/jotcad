#include <iostream>
#include <cassert>
#include "protocols.h"
#include "vfs_node.h"
#include "processor.h"
#include "data/shape.h"
#include "ops/box_op.h"
#include "ops/cut_op.h"
#include "ops/clean_op.h"
#include "ops/extrude_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config;
    config.id = "ghost-test";
    config.storage_dir = ".vfs_storage_ghost_test";
    fs::VFSNode vfs(config);

    // 1. Create a Box
    fs::Selector box_sel("jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}});
    BoxOp<>::execute(&vfs, box_sel, {0.0, 10.0}, {0.0, 10.0}, {0.0, 10.0});
    Shape box = vfs.read<Shape>(box_sel.with_output("$out"));

    // 2. Create a Cutter
    fs::Selector cutter_sel("jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 20.0}});
    BoxOp<>::execute(&vfs, cutter_sel, {0.0, 5.0}, {0.0, 5.0}, {-5.0, 15.0});
    Shape cutter = vfs.read<Shape>(cutter_sel.with_output("$out"));
    cutter.tf = Matrix::translate(FT(2.5), FT(2.5), FT(0));

    // 3. Test CUT (should leave a ghost)
    fs::Selector cut_sel("jot/cut");
    CutOp<>::execute(&vfs, cut_sel, box, {cutter}, false);
    Shape cut_group = vfs.read<Shape>(cut_sel.with_output("$out"));

    std::cout << "Cut Result Components: " << cut_group.components.size() << std::endl;
    assert(cut_group.components.size() == 2);
    assert(cut_group.components[1].is_ghost());
    std::cout << "Ghost confirmed in group." << std::endl;

    // 4. Test CLEAN
    fs::Selector clean_sel("jot/clean");
    CleanOp<>::execute(&vfs, clean_sel, cut_group, "ghost");
    Shape clean_res = vfs.read<Shape>(clean_sel.with_output("$out"));
    
    std::cout << "Clean Result Components: " << clean_res.components.size() << std::endl;
    assert(clean_res.components.size() == 1);
    std::cout << "Clean confirmed (ghost removed)." << std::endl;

    // 5. Test EXTRUDE (should ignore ghost)
    Shape circle_ghost;
    circle_ghost.add_tag("role", "ghost");
    circle_ghost.add_tag("type", "surface");
    circle_ghost.geometry = cutter.geometry; 

    fs::Selector fulfilling("test");
    ExtrudeOpBase<>::execute_sweep(&vfs, fulfilling, circle_ghost, Matrix::identity(), Matrix::translate(0,0,10));
    Shape extrude_ghost_res = vfs.read<Shape>(fulfilling.with_output("$out"));
    
    assert(extrude_ghost_res.is_ghost());
    assert(extrude_ghost_res.geometry == circle_ghost.geometry); 
    std::cout << "Extrude skipped ghost as expected." << std::endl;

    std::cout << "Ghost Workflow tests passed!" << std::endl;
    return 0;
}
