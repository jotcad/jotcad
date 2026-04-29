#include "test_base.h"
#include "cut_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("cut_3d_test");
    register_all_ops(&vfs);

    std::cout << "Testing 3D Cut Operation..." << std::endl;

    // 1. Create a large 10x10x10 Box
    Selector box1 = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Shape s1 = vfs.read<Shape>(box1);

    // 2. Create a small 5x5x20 tool Box that will cut through it
    Selector tool = Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 20.0}}}.with_output("$out");
    Shape t1 = vfs.read<Shape>(tool);
    // Offset tool so it's not perfectly aligned with subject corners
    t1.tf = Matrix::translate(FT(1), FT(1), FT(0)); 

    // 3. Execute Cut
    Selector cut1 = Selector{"jot/cut", {{"$in", box1}, {"tools", {vfs.materialize(t1)}}}}.with_output("$out");
    CutOp<>::execute(&vfs, cut1, vfs.read<Shape>(box1), {t1});
    Shape res = vfs.read<Shape>(cut1);

    assert(res.geometry.has_value());
    Geometry res_geo = vfs.read<Geometry>(res.geometry.value());

    std::cout << "  - Result vertices: " << res_geo.vertices.size() << std::endl;
    std::cout << "  - Result faces: " << res_geo.faces.size() << std::endl;

    // A 10x10 cube with an offset 5x5 square hole should have significantly more vertices and faces.
    assert(res_geo.vertices.size() > 8);
    assert(res_geo.faces.size() > 6);
    
    std::cout << "✅ 3D Cut PASS" << std::endl;
    return 0;
}
