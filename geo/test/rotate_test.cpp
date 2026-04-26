#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("rotate");
    register_all_ops(&vfs);
    
    std::cout << "Testing Rotate Operation..." << std::endl;
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    fs::Selector rot_addr = fs::Selector{"jot/rotate", {{"$in", box_addr}, {"angle", 45.0}}}.with_output("$out");
    Processor::execute(&vfs, rot_addr);
    
    Shape s = vfs.read<Shape>(rot_addr);
    vfs.verify_render(s, "rotate_op_45", "5f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f472e");

    std::cout << "✅ Rotate PASS" << std::endl;
    return 0;
}
