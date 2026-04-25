#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("outline");
    register_all_ops(&vfs);
    
    std::cout << "Testing Outline Operation..." << std::endl;
    
    fs::Selector box_fulfilling = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    Processor::execute(&vfs, box_fulfilling);
    Shape box = vfs.read<Shape>(box_fulfilling);
    
    fs::Selector outline_fulfilling = {"jot/outline", {{"$in", box_fulfilling}}};
    Processor::execute(&vfs, outline_fulfilling);
    
    Shape s = vfs.read<Shape>(outline_fulfilling);
    vfs.verify_render(s, "outline_op_basic", "26e192feee2a0153684ec08cb5fd51d38a69e1cc8676064675efabd297c5ea0a");

    std::cout << "✅ Outline PASS" << std::endl;
    return 0;
}
