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
    assert(result.components.size() == 2);
    
    Geometry g1 = vfs.read_geo(result.components[0].geometry);
    Geometry g2 = vfs.read_geo(result.components[1].geometry);
    
    std::cout << "Component 1 Geometry:" << std::endl;
    std::cout << g1.encode_text() << std::endl;
    std::cout << "Component 2 Geometry:" << std::endl;
    std::cout << g2.encode_text() << std::endl;
    
    assert(g1.faces[0].loops.size() == 1); // Box 1 notched
    assert(g1.vertices.size() == 8);
    assert(g2.faces[0].loops.size() == 1); // Box 2 notched
    assert(g2.vertices.size() == 8);
    
    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
