#include <iostream>
#include "../triangle_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_triangle_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Triangle Primitive..." << std::endl;
    
    fs::Selector fulfilling = {"jot/Triangle/equilateral", {{"size", {10.0}}}};
    TriangleEquilateralOp<>::execute(&vfs, fulfilling, {10.0});
    
    Shape s = vfs.read<Shape>(fulfilling);
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Triangle FAIL: No geometry handle returned" << std::endl;
        return 1;
    }
    
    Geometry g = vfs.read<Geometry>(s.geometry.value());
    if (g.vertices.size() != 3) {
        std::cerr << "❌ Triangle FAIL: Expected 3 vertices, got " << g.vertices.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Triangle PASS" << std::endl;
    return 0;
}
