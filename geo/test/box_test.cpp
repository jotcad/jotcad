#include <iostream>
#include "../box_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_box_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Box Primitive..." << std::endl;
    
    fs::Selector fulfilling = {"jot/Box", {{"size", {10.0, 10.0, 0.0}}}};
    BoxOp<>::execute(&vfs, fulfilling, {10.0, 10.0, 0.0});
    
    Shape s = vfs.read<Shape>(fulfilling);
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Box FAIL: No geometry handle returned" << std::endl;
        return 1;
    }
    
    Geometry g = vfs.read<Geometry>(s.geometry.value());
    if (g.vertices.size() != 4) {
        std::cerr << "❌ Box FAIL: Expected 4 vertices, got " << g.vertices.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Box PASS" << std::endl;
    return 0;
}
