#include "test_base.h"
#include "../box_op.h"
#include "../group_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Nested Boolean Cut (Group Subject)..." << std::endl;
    MockVFS vfs;

    // 1. Create 2 overlapping Boxes (50x50 each) starting at (0,0)
    Shape s1, s2;
    BoxOp<>::execute(&vfs, {50.0}, {50.0}, {0.0}, s1);
    BoxOp<>::execute(&vfs, {50.0}, {50.0}, {0.0}, s2);
    
    // Position them so they overlap. 
    // S1: x in [-15, 35], y in [0, 50]
    // S2: x in [15, 65], y in [0, 50]
    // Overlap: x in [15, 35], y in [0, 50]
    s1.tf = Matrix::translate(-15, 0, 0).to_vec();
    s2.tf = Matrix::translate(15, 0, 0).to_vec();

    // 2. Wrap them in a Group
    Shape group;
    GroupOp<>::execute(&vfs, {s1, s2}, group);
    
    // 3. Create a Tool (10x10 Box). Position at (20, 20) inside the overlap.
    Shape hole;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, hole);
    hole.tf = Matrix::translate(20, 20, 0).to_vec();

    // 4. Subtract from Group
    Shape result;
    CutOp<>::execute(&vfs, group, {hole}, result);
    
    // 5. Verify
    assert(result.components.size() == 2);
    
    Geometry g1 = vfs.read_geo(result.components[0].geometry);
    Geometry g2 = vfs.read_geo(result.components[1].geometry);
    
    // Both boxes should now have a hole loop
    assert(g1.faces[0].loops.size() == 2);
    assert(g2.faces[0].loops.size() == 2);
    
    std::cout << "✅ Nested Cut PASS" << std::endl;
    return 0;
}
