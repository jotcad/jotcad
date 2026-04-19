#include <iostream>
#include "../box_op.h"
#include "../cut_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_cut_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Cut Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Box/base", {{"size", {100.0, 100.0, 0.0}}}};
    BoxOp<>::execute(&vfs, base_sel, {100.0, 100.0, 0.0});
    Shape base = vfs.read<Shape>(base_sel);

    fs::Selector hole_sel = {"jot/Box/hole", {{"size", {10.0, 10.0, 0.0}}}};
    BoxOp<>::execute(&vfs, hole_sel, {10.0, 10.0, 0.0});
    Shape hole = vfs.read<Shape>(hole_sel);
    
    // Perform Cut
    fs::Selector cut_sel = {"jot/cut", {}};
    CutOp<>::execute(&vfs, cut_sel, base, {hole});
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Cut FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
