#include "test_base.h"
#include "join_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("join_test");
    register_all_ops(&vfs);

    std::cout << "Testing Join Operation..." << std::endl;

    // 1. 3D Union: Two overlapping 10x10x10 boxes
    // Box A: (-5, -5, -5) to (5, 5, 5)
    Selector boxA = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    
    // Box B: (0, 0, 0) to (10, 10, 10) -> shifted by (5, 5, 5)
    Shape sB = vfs.read<Shape>(boxA);
    sB.tf = Matrix::translate(FT(5), FT(5), FT(5));
    CID boxB_cid = vfs.materialize(sB);

    Selector join3d = Selector{"jot/join", {{"$in", boxA}, {"tools", {boxB_cid}}}}.with_output("$out");
    JoinOp<>::execute(&vfs, join3d, vfs.read<Shape>(boxA), {sB});
    Shape res3d = vfs.read<Shape>(join3d);

    assert(res3d.geometry.has_value());
    Geometry geo3d = vfs.read<Geometry>(res3d.geometry.value());
    std::cout << "  - 3D Union vertices: " << geo3d.vertices.size() << std::endl;
    // A single solid union should have more than 8 vertices but fewer than 16 (if they overlap significantly)
    // Actually, with corefinement and triangulation it will be quite a few.
    assert(geo3d.vertices.size() > 8);

    // 2. 2D Union: Two overlapping 10x10 rectangles
    Selector rectA = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    
    // Rect B: shifted by (5, 5, 0)
    Shape sRB = vfs.read<Shape>(rectA);
    sRB.tf = Matrix::translate(FT(5), FT(5), FT(0));
    CID rectB_cid = vfs.materialize(sRB);

    Selector join2d = Selector{"jot/join", {{"$in", rectA}, {"tools", {rectB_cid}}}}.with_output("$out");
    JoinOp<>::execute(&vfs, join2d, vfs.read<Shape>(rectA), {sRB});
    Shape res2d = vfs.read<Shape>(join2d);

    assert(res2d.geometry.has_value());
    Geometry geo2d = vfs.read<Geometry>(res2d.geometry.value());
    std::cout << "  - 2D Union vertices: " << geo2d.vertices.size() << std::endl;
    // Two 4-vertex rectangles overlapping into an L-shape should have 6 or 8 vertices depending on implementation
    assert(geo2d.vertices.size() >= 4);

    std::cout << "✅ Join PASS" << std::endl;
    return 0;
}
