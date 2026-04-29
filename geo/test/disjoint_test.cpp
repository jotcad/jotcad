#include "test_base.h"
#include "disjoint_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("disjoint_test");
    register_all_ops(&vfs);

    std::cout << "Testing Disjoint Operation..." << std::endl;

    // 1. Two overlapping component boxes
    // Child A: (0,0,0) to (10,10,10)
    // Child B: (5,5,5) to (15,15,15)
    Selector box_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Shape sA = vfs.read<Shape>(box_sel);
    sA.tf = Matrix::translate(FT(5), FT(5), FT(5)); // Centers A at (5,5,5) -> (0,0,0) to (10,10,10)
    
    Shape sB = sA;
    sB.tf = Matrix::translate(FT(10), FT(10), FT(10)); // Centers B at (10,10,10) -> (5,5,5) to (15,15,15)

    Shape parent;
    parent.components = {sA, sB};

    Selector disjoint_sel = Selector{"jot/disjoint", {{"$in", vfs.materialize(parent)}}}.with_output("$out");
    DisjointOp<>::execute(&vfs, disjoint_sel, parent);
    Shape res = vfs.read<Shape>(disjoint_sel);

    assert(res.components.size() == 2);
    
    // Child 1 (sB) should be untouched (8 vertices)
    Geometry geoB = vfs.read<Geometry>(res.components[1].geometry.value());
    std::cout << "  - Child 1 (B) vertices: " << geoB.vertices.size() << std::endl;
    assert(geoB.vertices.size() == 8);

    // Child 0 (sA) should be cut by B (should have more than 8 vertices)
    Geometry geoA = vfs.read<Geometry>(res.components[0].geometry.value());
    std::cout << "  - Child 0 (A) vertices: " << geoA.vertices.size() << std::endl;
    assert(geoA.vertices.size() > 8);

    // 2. Parent geometry vs child overlap
    Shape p2 = vfs.read<Shape>(box_sel); // Parent is 10x10 cube
    Shape c2 = p2;
    c2.tf = Matrix::translate(FT(5), FT(5), FT(5)); // Overlaps half the volume
    p2.components = {c2};

    Selector d2_sel = Selector{"jot/disjoint", {{"$in", vfs.materialize(p2)}}}.with_output("$out");
    DisjointOp<>::execute(&vfs, d2_sel, p2);
    Shape res2 = vfs.read<Shape>(d2_sel);

    // Parent should be cut
    Geometry p_geo = vfs.read<Geometry>(res2.geometry.value());
    std::cout << "  - Parent vertices: " << p_geo.vertices.size() << std::endl;
    assert(p_geo.vertices.size() > 8);

    std::cout << "✅ Disjoint PASS" << std::endl;
    return 0;
}
