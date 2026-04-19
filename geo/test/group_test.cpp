#include <iostream>
#include "../box_op.h"
#include "../group_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_group_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Group Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box/1", {{"size", {10, 10, 0}}}};
    BoxOp<>::execute(&vfs, s1_sel, {10, 10, 0});
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector s2_sel = {"jot/Box/2", {{"size", {20, 20, 0}}}};
    BoxOp<>::execute(&vfs, s2_sel, {20, 20, 0});
    Shape s2 = vfs.read<Shape>(s2_sel);
    
    fs::Selector group_sel = {"jot/group", {}};
    GroupOp<>::execute(&vfs, group_sel, {s1, s2});
    
    Shape out = vfs.read<Shape>(group_sel);
    if (out.components.size() != 2) {
        std::cerr << "❌ Group FAIL: Expected 2 components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Group PASS" << std::endl;
    return 0;
}
