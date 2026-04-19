#include <iostream>
#include "../hexagon_op.h"
#include "../corners_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_corners_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Corners Operation..." << std::endl;
    
    fs::Selector hex_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, hex_sel, 30.0);
    Shape hex = vfs.read<Shape>(hex_sel);
    
    fs::Selector corners_sel = {"jot/corners", {{"proxy", true}}};
    CornersOp<>::execute(&vfs, corners_sel, hex, true);
    
    Shape out = vfs.read<Shape>(corners_sel);
    if (out.components.size() != 6) {
        std::cerr << "❌ Corners FAIL: Expected 6 corner components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Corners PASS" << std::endl;
    return 0;
}
