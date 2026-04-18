#include "test_base.h"
#include "../hexagon_op.h"
#include "../rotate_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Rotate..." << std::endl;
    MockVFS vfs;
    
    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    Shape rotated;
    RotateOp<>::execute(&vfs, {hex}, {0.25}, 2, rotated);
    
    assert(std::abs(rotated.tf[0]) < 1e-6); 
    assert(std::abs(rotated.tf[1] + 1.0) < 1e-6);
    
    std::cout << "✅ Rotate PASS" << std::endl;
    return 0;
}
