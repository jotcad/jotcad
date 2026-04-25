#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut");
    register_all_ops(&vfs);
    
    std::cout << "Testing Cut Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Box", {{"width", 100.0}, {"height", 100.0}, {"depth", 0.0}, {"id", "base"}}};
    Processor::execute(&vfs, base_sel);
    Shape base = vfs.read<Shape>(base_sel);

    fs::Selector hole_sel = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}, {"id", "hole"}}};
    Processor::execute(&vfs, hole_sel);
    Shape hole = vfs.read<Shape>(hole_sel);
    
    // Perform Cut
    fs::Selector cut_sel = {"jot/cut", {{"$in", base_sel}, {"tools", {hole_sel}}}};
    Processor::execute(&vfs, cut_sel);
    
    Shape out = vfs.read<Shape>(cut_sel);
    vfs.verify_render(out, "cut_op_basic", "cc93abc575c112b51cd4aa1f1f57cbe555d6eed21f49b3f6d173522dde540921");

    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
