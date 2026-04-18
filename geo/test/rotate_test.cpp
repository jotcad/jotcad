#include "test_base.h"
#include "../rotate_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Rotate..." << std::endl;
    MockVFS vfs;

    Shape s1;
    s1.tf = Matrix::identity().to_vec();

    Shape rotated;
    // Rotate 1/4 turn (90 deg) around Z. 
    // From observed matrix:
    // [  0.00  1.00  0.00  0.00 ]
    // [ -1.00 -0.00 -0.00 -0.00 ]
    // Index 1 (m01) = 1.0
    // Index 4 (m10) = -1.0
    RotateOp<>::execute(&vfs, {s1}, {0.25}, 2, rotated);

    assert(std::abs(rotated.tf[1] - 1.0) < 1e-6);
    assert(std::abs(rotated.tf[4] + 1.0) < 1e-6);

    std::cout << "✅ Rotate PASS" << std::endl;
    return 0;
}
