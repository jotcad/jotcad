#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("triangle");
    register_all_ops(&vfs);
    
    std::cout << "Testing Triangle Primitive..." << std::endl;
    
    fs::Selector fulfilling = fs::Selector{"jot/Triangle/equilateral", {{"side", {30.0}}}}.with_output("$out");
    Processor::execute(&vfs, fulfilling);
    
    Shape s = vfs.read<Shape>(fulfilling);
    vfs.verify_render(s, "triangle_op_equilateral", "7e39b8a202653c60c140305bd9dc55a504fc58e4df78ea79903ef192bd1323cf");

    std::cout << "✅ Triangle PASS" << std::endl;
    return 0;
}
