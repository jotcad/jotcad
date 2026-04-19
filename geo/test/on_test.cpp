#include <iostream>
#include "../hexagon_op.h"
#include "../corners_op.h"
#include "../on_op.h"
#include "../offset_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_on_test"};
    fs::VFSNode vfs(config);

    // Register ops
    hexagon_init();
    corners_init();
    offset_init();
    on_init();

    // Map Processor Registry to VFS Node
    for (auto const& [path, op] : Processor::registry_instance()) {
        vfs.register_op(path, [&vfs, path](const fs::VFSNode::VFSRequest& req) {
            return Processor::registry_instance()[path].handler(&vfs, req);
        }, op.schema);
    }
    
    std::cout << "Testing On (Conjugation) Operation..." << std::endl;
    
    fs::Selector hex_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, hex_sel, 30.0);
    Shape hex = vfs.read<Shape>(hex_sel);
    
    fs::Selector corners_sel = {"jot/corners", {{"proxy", false}}};
    CornersOp<>::execute(&vfs, corners_sel, hex, false);
    Shape corners = vfs.read<Shape>(corners_sel);
    
    // Operation to apply: offset(2) with explicit $in
    fs::Selector op = {"jot/offset", {{"$in", "$in"}, {"diameter", 2.0}}};
    
    fs::Selector on_sel = {"jot/on", {}};
    OnOp<>::execute(&vfs, on_sel, hex, corners, op);
    
    Shape out = vfs.read<Shape>(on_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ On FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ On PASS" << std::endl;
    return 0;
}
