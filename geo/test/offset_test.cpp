#include <iostream>
#include "../hexagon_op.h"
#include "../offset_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_offset_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Offset Operation..." << std::endl;
    
    fs::Selector hex_fulfilling = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, hex_fulfilling, 30.0);
    Shape hex = vfs.read<Shape>(hex_fulfilling);
    
    fs::Selector offset_fulfilling = {"jot/offset", {{"diameter", 5.0}}};
    OffsetOp<>::execute(&vfs, offset_fulfilling, hex, 5.0);
    
    Shape s = vfs.read<Shape>(offset_fulfilling);
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Offset FAIL: No geometry handle returned" << std::endl;
        return 1;
    }
    
    Geometry g = vfs.read<Geometry>(s.geometry.value());
    // Offset shouldn't change vertex count for a simple polygon in many implementations, 
    // but here we just check if it exists and can be read back.
    if (g.vertices.size() < 6) {
        std::cerr << "❌ Offset FAIL: Unexpected vertex count " << g.vertices.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Offset PASS" << std::endl;
    return 0;
}
