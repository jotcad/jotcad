#include "test_base.h"
#include "../box_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing 2D Boolean Cut..." << std::endl;
    MockVFS vfs;

    // 1. Create a 100x100 Base Box at (0,0)
    Shape base;
    BoxOp<>::execute(&vfs, {100.0}, {100.0}, {0.0}, base);
    
    // 2. Create a 10x10 Hole Box centered at (50,50)
    Shape hole;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, hole);
    hole.tf = Matrix::translate(45, 45, 0).to_vec(); // Box(10,10) is at (0,0). Move corner to (45,45) so center is (50,50)

    // 3. Subtract
    Shape result;
    CutOp<>::execute(&vfs, base, {hole}, result);

    // 4. Verify
    assert(result.geometry.has_value());
    Geometry geo = vfs.read_geo(result.geometry);

    // Should have 1 face with 2 loops (1 outer + 1 internal hole)
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 2);

    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
