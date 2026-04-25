#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("rotate");
    register_all_ops(&vfs);
    
    std::cout << "Testing Rotate Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}};
    Processor::execute(&vfs, s1_sel);
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector rotate_sel = {"jot/rotate", {{"$in", s1_sel}, {"angle", 90.0}}};
    Processor::execute(&vfs, rotate_sel);
    
    Shape out = vfs.read<Shape>(rotate_sel);
    if (out.tf.size() != 16) {
        std::cerr << "❌ Rotate FAIL: Invalid transform matrix" << std::endl;
        return 1;
    }

    std::cout << "✅ Rotate PASS" << std::endl;
    return 0;
}
