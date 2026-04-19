#include <iostream>
#include "../box_op.h"
#include "../on_op.h"
#include "../offset_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_acc_test"};
    fs::VFSNode vfs(config);
    
    // Register required ops for the recipe
    offset_init();
    on_init();
    box_init();

    // Map Processor Registry to VFS Node
    for (auto const& [path, op] : Processor::registry_instance()) {
        vfs.register_op(path, [&vfs, path](const fs::VFSNode::VFSRequest& req) {
            return Processor::registry_instance()[path].handler(&vfs, req);
        }, op.schema);
    }
    
    std::cout << "Testing Accumulator (On) Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Box/base", {{"size", {50.0, 50.0, 0.0}}}};
    BoxOp<>::execute(&vfs, base_sel, {50.0, 50.0, 0.0});
    Shape base = vfs.read<Shape>(base_sel);

    // Create target points
    Shape targets;
    targets.components.resize(3);
    targets.components[0].tf = Matrix::translate(10, 0, 0).to_vec();
    targets.components[0].geometry = base.geometry;
    targets.components[1].tf = Matrix::translate(20, 0, 0).to_vec();
    targets.components[1].geometry = base.geometry;
    targets.components[2].tf = Matrix::translate(30, 0, 0).to_vec();
    targets.components[2].geometry = base.geometry;

    // Operation: offset(2) with explicit $in placeholder
    fs::Selector recipe = {"jot/offset", {{"$in", "$in"}, {"diameter", 2.0}}};
    
    fs::Selector on_sel = {"jot/on", {}};
    OnOp<>::execute(&vfs, on_sel, base, targets, recipe);
    
    Shape out = vfs.read<Shape>(on_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Accumulator FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Accumulator PASS" << std::endl;
    return 0;
}
