#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("box");
    register_all_ops(&vfs);
    
    std::cout << "Testing Box Primitive..." << std::endl;
    
    // 1. 2D Box
    fs::Selector box2d_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box2d_addr);
    Shape s2d = vfs.read<Shape>(box2d_addr);
    vfs.verify_render(s2d, "box_op_2d", "911540d06dff354cf8f9cad5a32d5d700c4670df28ad720cd6ba7688d0de0df2");

    // 2. 3D Box
    fs::Selector box3d_addr = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, box3d_addr);
    Shape s3d = vfs.read<Shape>(box3d_addr);
    vfs.verify_render(s3d, "box_op_3d", "95e7a44b7954f9faa3839b5fee1c67161510081c5d472562b742a4876105f06b");
    
    Geometry box_geo = vfs.read<Geometry>(s3d.geometry.value());
    vfs.verify_well_formed_solid(box_geo, "3D Box (10x10x10)");

    std::cout << "✅ ALL Box Tests Passed" << std::endl;
    return 0;
}
