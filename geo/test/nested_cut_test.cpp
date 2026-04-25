#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("nested_cut");
    register_all_ops(&vfs);
    
    std::cout << "Testing Nested Cut Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box", {{"width", 50.0}, {"height", 50.0}, {"depth", 0.0}, {"id", 1}}};
    Processor::execute(&vfs, s1_sel);
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector s2_sel = {"jot/Box", {{"width", 50.0}, {"height", 50.0}, {"depth", 0.0}, {"id", 2}}};
    Processor::execute(&vfs, s2_sel);
    Shape s2 = vfs.read<Shape>(s2_sel);
    
    fs::Selector group_sel = {"jot/Group", {{"shapes", {s1_sel, s2_sel}}}};
    Processor::execute(&vfs, group_sel);
    Shape group = vfs.read<Shape>(group_sel);

    fs::Selector hole_sel = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}, {"id", "hole"}}};
    Processor::execute(&vfs, hole_sel);
    Shape hole = vfs.read<Shape>(hole_sel);
    
    fs::Selector cut_sel = {"jot/cut", {{"$in", group_sel}, {"tools", {hole_sel}}}};
    Processor::execute(&vfs, cut_sel);
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (out.components.size() != 2) {
        std::cerr << "❌ Nested Cut FAIL: Expected 2 components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
