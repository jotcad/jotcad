#include "test_base.h"
#include "transform_ops.h"
#include "box_op.h"
#include "move_op.h"
#include "rotate_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("transform_test");
    register_all_ops(&vfs);

    // 1. Setup initial box at (10, 0, 0)
    // NOTE: We must use .with_output("$out") because operators typically write to $out.
    fs::Selector box_sel = fs::Selector("jot/Box", {{"width", 1.0}, {"height", 1.0}, {"depth", 1.0}}).with_output("$out");
    fs::Selector move_sel = fs::Selector("jot/moveX", {{"$in", box_sel}, {"x", {10.0}}}).with_output("$out");
    Shape a = vfs.read<Shape>(move_sel);
    
    std::cout << "Shape A (initial) tf: " << a.tf.to_vec() << std::endl;

    // 2. Setup target B at (0, 10, 0)
    fs::Selector target_b_sel = fs::Selector("jot/moveY", {{"$in", box_sel}, {"y", {10.0}}}).with_output("$out");
    Shape b = vfs.read<Shape>(target_b_sel);
    
    // 3. Setup target C at (0, 0, 10)
    fs::Selector target_c_sel = fs::Selector("jot/moveZ", {{"$in", box_sel}, {"z", {10.0}}}).with_output("$out");
    Shape c = vfs.read<Shape>(target_c_sel);

    // 4. Test 'to' with multiple targets: a.to(b, c)
    fs::Selector to_multi_sel = fs::Selector("jot/to", {{"$in", move_sel}, {"targets", {target_b_sel, target_c_sel}}}).with_output("$out");
    Shape to_group = vfs.read<Shape>(to_multi_sel);
    
    std::cout << "a.to(b, c) components: " << to_group.components.size() << std::endl;
    if (to_group.components.size() != 2) {
        std::cerr << "FAIL: a.to(b, c) should produce 2 components!" << std::endl;
        return 1;
    }
    
    if (to_group.components[0].tf.to_vec() != b.tf.to_vec()) {
        std::cerr << "FAIL: first component of a.to(b, c) mismatch!" << std::endl;
        std::cerr << "Actual: " << to_group.components[0].tf.to_vec() << std::endl;
        std::cerr << "Expected: " << b.tf.to_vec() << std::endl;
        return 1;
    }
    if (to_group.components[1].tf.to_vec() != c.tf.to_vec()) {
        std::cerr << "FAIL: second component of a.to(b, c) mismatch!" << std::endl;
        std::cerr << "Actual: " << to_group.components[1].tf.to_vec() << std::endl;
        std::cerr << "Expected: " << c.tf.to_vec() << std::endl;
        return 1;
    }

    // 5. Test 'origin' operator: a.origin()
    fs::Selector origin_sel = fs::Selector("jot/origin", {{"$in", move_sel}}).with_output("$out");
    Shape a_origin = vfs.read<Shape>(origin_sel);
    if (a_origin.tf.to_vec() != Matrix::identity().to_vec()) {
        std::cerr << "FAIL: a.origin() transformation mismatch!" << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: Multi-target transform operators verified." << std::endl;
    return 0;
}
