#include <iostream>
#include "../box_op.h"
#include "../group_op.h"
#include "../cut_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_nested_cut_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Nested Cut Operation..." << std::endl;
    
    fs::Selector s1_sel = {"jot/Box/1", {{"size", {50.0, 50.0, 0.0}}}};
    BoxOp<>::execute(&vfs, s1_sel, {50.0, 50.0, 0.0});
    Shape s1 = vfs.read<Shape>(s1_sel);

    fs::Selector s2_sel = {"jot/Box/2", {{"size", {50.0, 50.0, 0.0}}}};
    BoxOp<>::execute(&vfs, s2_sel, {50.0, 50.0, 0.0});
    Shape s2 = vfs.read<Shape>(s2_sel);
    
    fs::Selector group_sel = {"jot/group", {}};
    GroupOp<>::execute(&vfs, group_sel, {s1, s2});
    Shape group = vfs.read<Shape>(group_sel);

    fs::Selector hole_sel = {"jot/Box/hole", {{"size", {10.0, 10.0, 0.0}}}};
    BoxOp<>::execute(&vfs, hole_sel, {10.0, 10.0, 0.0});
    Shape hole = vfs.read<Shape>(hole_sel);
    
    fs::Selector cut_sel = {"jot/cut", {}};
    CutOp<>::execute(&vfs, cut_sel, group, {hole});
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (out.components.size() != 2) {
        std::cerr << "❌ Nested Cut FAIL: Expected 2 components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
