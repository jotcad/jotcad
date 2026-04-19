#include <iostream>
#include "../box_op.h"
#include "../outline_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_outline_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Outline Operation..." << std::endl;
    
    fs::Selector box_fulfilling = {"jot/Box", {{"size", {10.0, 10.0, 10.0}}}};
    BoxOp<>::execute(&vfs, box_fulfilling, {10.0, 10.0, 10.0});
    Shape box = vfs.read<Shape>(box_fulfilling);
    
    fs::Selector outline_fulfilling = {"jot/outline", {{"$in", "jot/Box"}}};
    OutlineOp<>::execute(&vfs, outline_fulfilling, box);
    
    Shape s = vfs.read<Shape>(outline_fulfilling);
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Outline FAIL: No geometry handle returned" << std::endl;
        return 1;
    }
    
    Geometry g = vfs.read<Geometry>(s.geometry.value());
    if (g.segments.empty()) {
        std::cerr << "❌ Outline FAIL: No segments generated" << std::endl;
        return 1;
    }

    std::cout << "✅ Outline PASS" << std::endl;
    return 0;
}
