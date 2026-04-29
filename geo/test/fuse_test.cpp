#include "test_base.h"
#include "fuse_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("fuse_test");
    register_all_ops(&vfs);

    std::cout << "Testing Fuse Operation..." << std::endl;

    // 1. 3D Overlapping Fuse: Two 10x10x10 boxes
    Selector boxA_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Shape sA = vfs.read<Shape>(boxA_sel);

    Shape sB = sA;
    sB.tf = Matrix::translate(FT(5), FT(5), FT(5));
    CID boxB_cid = vfs.materialize(sB);

    Selector fuse3d = Selector{"jot/fuse", {{"$in", boxA_sel}, {"tools", {boxB_cid}}}}.with_output("$out");
    FuseOp<>::execute(&vfs, fuse3d, sA, {sB});
    Shape res3d = vfs.read<Shape>(fuse3d);

    assert(res3d.geometry.has_value());
    assert(res3d.components.empty()); // Must be flattened
    
    Geometry geo3d = vfs.read<Geometry>(res3d.geometry.value());
    std::cout << "  - 3D Fused vertices: " << geo3d.vertices.size() << std::endl;
    // Overlapping cubes fused should result in a single solid shell.
    // 20 vertices is expected for this specific overlap in CGAL's union.
    assert(geo3d.vertices.size() > 8);

    // 2. Disjoint 3D Fuse
    Shape sC = sA;
    sC.tf = Matrix::translate(FT(50), FT(0), FT(0));
    
    Selector fuse_disjoint = Selector{"jot/fuse", {{"$in", boxA_sel}, {"tools", {vfs.materialize(sC)}}}}.with_output("$out");
    FuseOp<>::execute(&vfs, fuse_disjoint, sA, {sC});
    Shape res_disjoint = vfs.read<Shape>(fuse_disjoint);
    
    Geometry geo_disjoint = vfs.read<Geometry>(res_disjoint.geometry.value());
    std::cout << "  - Disjoint Fused vertices: " << geo_disjoint.vertices.size() << std::endl;
    // Two non-overlapping 8-vertex boxes fused should have exactly 16 vertices.
    assert(geo_disjoint.vertices.size() == 16);

    std::cout << "✅ Fuse PASS" << std::endl;
    return 0;
}
