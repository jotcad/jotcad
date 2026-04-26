#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("group");
    register_all_ops(&vfs);
    
    std::cout << "Testing Group Operation..." << std::endl;
    
    fs::Selector s1_addr = fs::Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 0.0}}}.with_output("$out");
    fs::Selector s2_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    
    Processor::execute(&vfs, s1_addr);
    Processor::execute(&vfs, s2_addr);
    
    fs::Selector group_addr = fs::Selector{"jot/and", {{"$in", s1_addr}, {"shapes", {s1_addr, s2_addr}}}}.with_output("$out");
    Processor::execute(&vfs, group_addr);
    
    Shape g = vfs.read<Shape>(group_addr);
    if (g.components.size() != 2) {
        std::cerr << "❌ Group FAIL: Expected 2 components, got " << g.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Group PASS" << std::endl;
    return 0;
}
