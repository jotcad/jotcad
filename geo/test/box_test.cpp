#include "test_base.h"
#include "../box_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Box Primitive..." << std::endl;
    
    // 1. 2D Box
    fs::Selector box2d_addr = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}};
    BoxOp<>::execute(&vfs, box2d_addr, 10.0, 10.0, 0.0);
    Shape s2d = vfs.read<Shape>(box2d_addr);
    vfs.verify_render(s2d, "box_op_2d", "911540d06dff354cf8f9cad5a32d5d700c4670df28ad720cd6ba7688d0de0df2");

    // 2. 3D Box
    fs::Selector box3d_addr = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    BoxOp<>::execute(&vfs, box3d_addr, 10.0, 10.0, 10.0);
    Shape s3d = vfs.read<Shape>(box3d_addr);
    vfs.verify_render(s3d, "box_op_3d", "95e7a44b7954f9faa3839b5fee1c67161510081c5d472562b742a4876105f06b");

    std::cout << "✅ ALL Box Tests Passed" << std::endl;
    return 0;
}
