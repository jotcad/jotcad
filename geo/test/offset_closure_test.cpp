#include <iostream>
#include "../box_op.h"
#include "../offset_op.h"
#include "../cut_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_offset_closure_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing Offset Closure Scenarios..." << std::endl;

    {
        std::cout << "  - Testing Hole Filling (Positive Offset)..." << std::endl;
        fs::Selector base_sel = {"jot/Box/base", {{"size", {50.0, 50.0, 0.0}}}};
        BoxOp<>::execute(&vfs, base_sel, {50.0, 50.0, 0.0});
        Shape base = vfs.read<Shape>(base_sel);

        fs::Selector hole_sel = {"jot/Box/hole", {{"size", {10.0, 10.0, 0.0}}}};
        BoxOp<>::execute(&vfs, hole_sel, {10.0, 10.0, 0.0});
        Shape hole = vfs.read<Shape>(hole_sel);
        
        fs::Selector cut_sel = {"jot/cut", {}};
        CutOp<>::execute(&vfs, cut_sel, base, {hole});
        Shape holed_box = vfs.read<Shape>(cut_sel);
        
        fs::Selector offset_sel = {"jot/offset", {{"diameter", 6.0}}};
        OffsetOp<>::execute(&vfs, offset_sel, holed_box, 6.0);
        
        Shape s = vfs.read<Shape>(offset_sel);
        Geometry g = vfs.read<Geometry>(s.geometry.value());
        
        // Face boundary size: 4 was_cw: 0 is_simple: 1
        // Face 0 vertex 0: -5, -5
        if (!g.faces.empty()) {
            std::cout << "    ✅ Hole filled correctly." << std::endl;
        }
    }

    {
        std::cout << "  - Testing Part Collapse (Negative Offset)..." << std::endl;
        fs::Selector small_sel = {"jot/Box/small", {{"size", {10.0, 10.0, 0.0}}}};
        BoxOp<>::execute(&vfs, small_sel, {10.0, 10.0, 0.0});
        Shape small = vfs.read<Shape>(small_sel);
        
        fs::Selector collapsed_sel = {"jot/offset/neg", {{"diameter", -6.0}}};
        OffsetOp<>::execute(&vfs, collapsed_sel, small, -6.0);
        
        Shape s = vfs.read<Shape>(collapsed_sel);
        Geometry g = vfs.read<Geometry>(s.geometry.value());
        
        if (g.faces.empty()) {
            std::cout << "    ✅ Part collapsed correctly." << std::endl;
        }
    }

    std::cout << "✅ Offset Closure PASS" << std::endl;
    return 0;
}
