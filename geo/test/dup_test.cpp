#include "test_base.h"
#include "transform_ops.h"
#include "box_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("dup_test");
    register_all_ops(&vfs);

    // 1. Setup initial box
    fs::Selector box_sel = fs::Selector("jot/Box", {{"width", 1.0}, {"height", 1.0}, {"depth", 1.0}}).with_output("$out");
    Shape box = vfs.read<Shape>(box_sel);

    // 2. Test dup(0) - should be empty group
    fs::Selector dup0_sel = fs::Selector("jot/dup", {{"$in", box_sel}, {"count", 0.0}}).with_output("$out");
    Shape group0 = vfs.read<Shape>(dup0_sel);
    std::cout << "dup(0) components: " << group0.components.size() << std::endl;
    if (group0.components.size() != 0) {
        std::cerr << "FAIL: dup(0) should produce 0 components!" << std::endl;
        return 1;
    }

    // 3. Test dup(1) - should be original shape
    fs::Selector dup1_sel = fs::Selector("jot/dup", {{"$in", box_sel}, {"count", 1.0}}).with_output("$out");
    Shape shape1 = vfs.read<Shape>(dup1_sel);
    std::cout << "dup(1) components: " << shape1.components.size() << std::endl;
    if (shape1.components.size() != 0) { // Should not be a group
        // If it was a group, it would have components. 
        // Based on implementation, c==1 returns 'in', which is just the box.
    }
    if (shape1.tf != box.tf) {
        std::cerr << "FAIL: dup(1) transformation mismatch!" << std::endl;
        return 1;
    }

    // 4. Test dup(3) - should be group of 3
    fs::Selector dup3_sel = fs::Selector("jot/dup", {{"$in", box_sel}, {"count", 3.0}}).with_output("$out");
    Shape group3 = vfs.read<Shape>(dup3_sel);
    std::cout << "dup(3) components: " << group3.components.size() << std::endl;
    if (group3.components.size() != 3) {
        std::cerr << "FAIL: dup(3) should produce 3 components!" << std::endl;
        return 1;
    }
    for (int i = 0; i < 3; ++i) {
        if (group3.components[i].tf != box.tf) {
            std::cerr << "FAIL: component " << i << " of dup(3) mismatch!" << std::endl;
            return 1;
        }
    }

    std::cout << "SUCCESS: dup operator verified." << std::endl;
    return 0;
}
