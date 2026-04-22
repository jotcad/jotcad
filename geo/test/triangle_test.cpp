#include "test_base.h"
#include "../triangle_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Triangle Primitive..." << std::endl;
    
    fs::Selector fulfilling = {"jot/Triangle/equilateral", {{"size", {10.0}}}};
    TriangleEquilateralOp<>::execute(&vfs, fulfilling, {10.0});
    
    Shape s = vfs.read<Shape>(fulfilling);
    vfs.verify_render(s, "triangle_op_equilateral", "dcc5a7ad236cdfcbe1efcebf38dc82fb9190394526ae8bf4158270ca0405569c");

    std::cout << "✅ Triangle PASS" << std::endl;
    return 0;
}
