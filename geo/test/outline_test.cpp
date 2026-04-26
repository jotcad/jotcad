#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("outline");
    register_all_ops(&vfs);
    
    std::cout << "Testing Outline Operation..." << std::endl;
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    fs::Selector out_addr = fs::Selector{"jot/outline", {{"$in", box_addr}}}.with_output("$out");
    Processor::execute(&vfs, out_addr);
    
    Shape s = vfs.read<Shape>(out_addr);
    vfs.verify_render(s, "outline_op_basic", "4f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f472d");

    std::cout << "✅ Outline PASS" << std::endl;
    return 0;
}
