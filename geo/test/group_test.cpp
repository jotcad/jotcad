#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("group");
    register_all_ops(&vfs);
    
    std::cout << "Testing Group Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}, {"id", 1}}};
    Processor::execute(&vfs, s1_sel);
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector s2_sel = {"jot/Box", {{"width", 20.0}, {"height", 20.0}, {"depth", 0.0}, {"id", 2}}};
    Processor::execute(&vfs, s2_sel);
    Shape s2 = vfs.read<Shape>(s2_sel);
    
    // Group (uppercase) has no $in, just shapes
    fs::Selector group_sel = {"jot/Group", {{"shapes", {s1_sel, s2_sel}}}};
    Processor::execute(&vfs, group_sel);
    
    Shape out = vfs.read<Shape>(group_sel);
    if (out.components.size() != 2) {
        std::cerr << "❌ Group FAIL: Expected 2 components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Group PASS" << std::endl;
    return 0;
}
