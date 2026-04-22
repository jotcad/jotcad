#include "test_base.h"
#include "../box_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Cut Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Box/base", {{"width", 100.0}, {"height", 100.0}, {"depth", 0.0}}};
    BoxOp<>::execute(&vfs, base_sel, 100.0, 100.0, 0.0);
    Shape base = vfs.read<Shape>(base_sel);

    fs::Selector hole_sel = {"jot/Box/hole", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}};
    BoxOp<>::execute(&vfs, hole_sel, 10.0, 10.0, 0.0);
    Shape hole = vfs.read<Shape>(hole_sel);
    
    // Perform Cut
    fs::Selector cut_sel = {"jot/cut", {}};
    CutOp<>::execute(&vfs, cut_sel, base, {hole});
    
    Shape out = vfs.read<Shape>(cut_sel);
    vfs.verify_render(out, "cut_op_basic", "cc93abc575c112b51cd4aa1f1f57cbe555d6eed21f49b3f6d173522dde540921");

    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
