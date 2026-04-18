#include "test_base.h"
#include "../box_op.h"
#include "../group_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Nested Boolean Cut (Group Subject)..." << std::endl;
    MockVFS vfs;
    register_all_ops();

    for (auto const& [path, op] : Processor::registry()) {
        vfs.register_op(path, [&vfs, path](const VFSNode::VFSRequest& req) {
            return Processor::registry()[path].logic(&vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }
    
    // 1. Create a Group of two boxes
    Shape s1;
    BoxOp<>::execute(&vfs, {50.0}, {50.0}, {0.0}, s1);
    s1.tf = Matrix::translate(-25, 0, 0).to_vec();
    
    Shape s2;
    BoxOp<>::execute(&vfs, {50.0}, {50.0}, {0.0}, s2);
    s2.tf = Matrix::translate(25, 0, 0).to_vec();

    Shape group;
    GroupOp<>::execute(&vfs, {s1, s2}, {}, group);
    // group.geometry.path is empty
    
    // 2. Create a hole in the middle (where they overlap)
    Shape hole;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, hole);
    
    // 3. Subtract from Group
    Shape result;
    CutOp<>::execute(&vfs, group, {hole}, result);
    
    // 4. Verify
    Geometry geo = vfs.read_geo(result.geometry);
    // Should have 1 face (they merged) with 1 hole
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 2);
    
    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
