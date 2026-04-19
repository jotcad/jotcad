#include <iostream>
#include "../hexagon_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_hexagon_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Hexagon Primitive..." << std::endl;
    
    fs::Selector fulfilling = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, fulfilling, 30.0);
    
    Shape s = vfs.read<Shape>(fulfilling);
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Hexagon FAIL: No geometry handle returned" << std::endl;
        return 1;
    }
    
    Geometry g = vfs.read<Geometry>(s.geometry.value());
    if (g.vertices.size() != 6) {
        std::cerr << "❌ Hexagon FAIL: Expected 6 vertices, got " << g.vertices.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Hexagon PASS" << std::endl;
    return 0;
}
