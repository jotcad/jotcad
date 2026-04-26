#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("nested_cut");
    register_all_ops(&vfs);
    
    std::cout << "Testing Nested Cut Operation..." << std::endl;
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 30.0}, {"height", 30.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    fs::Selector t1_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, t1_addr);
    
    fs::Selector cut1_addr = fs::Selector{"jot/cut", {{"$in", box_addr}, {"tools", {t1_addr}}}}.with_output("$out");
    Processor::execute(&vfs, cut1_addr);
    
    fs::Selector t2_addr = fs::Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, t2_addr);
    
    fs::Selector cut2_addr = fs::Selector{"jot/cut", {{"$in", cut1_addr}, {"tools", {t2_addr}}}}.with_output("$out");
    Processor::execute(&vfs, cut2_addr);
    
    Shape s = vfs.read<Shape>(cut2_addr);
    vfs.verify_render(s, "nested_cut_op", "7f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f4730");

    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
