#include <iostream>
#include "../hexagon_op.h"
#include "../triangle_op.h"
#include "../cut_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_cut_hex_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Cut Hexagon Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Hexagon/base", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, base_sel, 30.0);
    Shape base = vfs.read<Shape>(base_sel);

    fs::Selector tool_sel = {"jot/Triangle/tool", {{"size", {2.0}}}};
    TriangleEquilateralOp<>::execute(&vfs, tool_sel, {2.0});
    Shape tool = vfs.read<Shape>(tool_sel);
    
    fs::Selector cut_sel = {"jot/cut", {}};
    CutOp<>::execute(&vfs, cut_sel, base, {tool});
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Cut Hexagon FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Cut Hexagon PASS" << std::endl;
    return 0;
}
