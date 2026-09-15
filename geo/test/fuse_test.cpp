#include "test_base.h"
#include "fuse_op.h"
#include "fix/assert_mesh.h"

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
    assert(geo3d.vertices.size() == 20);

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

    // 3. 3-Box Orthogonal Caltrop Cross Fuse
    Selector boxX_sel = Selector{"jot/Box", {{"width", 30.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Selector boxY_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 30.0}, {"depth", 10.0}}}.with_output("$out");
    Selector boxZ_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 30.0}}}.with_output("$out");

    Shape sX = vfs.read<Shape>(boxX_sel);
    Shape sY = vfs.read<Shape>(boxY_sel);
    Shape sZ = vfs.read<Shape>(boxZ_sel);

    Selector fuse_cross = Selector{"jot/Fuse", {{"shapes", {vfs.materialize(sX), vfs.materialize(sY), vfs.materialize(sZ)}}}}.with_output("$out");
    FusePrimitiveOp<>::execute(&vfs, fuse_cross, {sX, sY, sZ});
    Shape res_cross = vfs.read<Shape>(fuse_cross);

    assert(res_cross.geometry.has_value());
    assert(res_cross.components.empty());

    Geometry geo_cross = vfs.read<Geometry>(res_cross.geometry.value());
    std::cout << "  - 3-Box Caltrop Cross Fused vertices: " << geo_cross.vertices.size() << std::endl;

    ExactMesh m_cross = boolean::Engine::geometry_to_mesh(geo_cross);
    std::cout << "  - m_cross is_closed=" << CGAL::is_closed(m_cross)
              << " does_self_intersect=" << CGAL::Polygon_mesh_processing::does_self_intersect(m_cross)
              << " bounds_volume=" << CGAL::Polygon_mesh_processing::does_bound_a_volume(m_cross)
              << std::endl;

    fix::assert_well_formed_closed_mesh(m_cross, "m_cross in fuse_test");

    std::cout << "✅ Fuse PASS" << std::endl;
    return 0;
}
