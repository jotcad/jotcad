#include <iostream>
#include "../box_op.h"
#include "../rotate_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_rotate_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Rotate Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box/1", {{"size", {10, 10, 0}}}};
    BoxOp<>::execute(&vfs, s1_sel, {10, 10, 0});
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector rotate_sel = {"jot/rotate", {{"angle", 90.0}}};
    RotateOp<>::execute(&vfs, rotate_sel, s1, 90.0);
    
    Shape out = vfs.read<Shape>(rotate_sel);
    if (out.tf.size() != 16) {
        std::cerr << "❌ Rotate FAIL: Invalid transform matrix" << std::endl;
        return 1;
    }

    std::cout << "✅ Rotate PASS" << std::endl;
    return 0;
}
