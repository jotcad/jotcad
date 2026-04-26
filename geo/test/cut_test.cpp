#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut");
    register_all_ops(&vfs);
    
    std::cout << "Testing Cut Operation..." << std::endl;
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 20.0}, {"height", 20.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    fs::Selector tool_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, tool_addr);
    
    fs::Selector cut_addr = fs::Selector{"jot/cut", {{"$in", box_addr}, {"tools", {tool_addr}}}}.with_output("$out");
    Processor::execute(&vfs, cut_addr);
    
    Shape s = vfs.read<Shape>(cut_addr);
    vfs.verify_render(s, "cut_op_basic", "6f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f472f");

    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
