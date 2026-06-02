#include "ops/obb_op.h"
#include "ops/box_op.h"
#include "ops/rotate_op.h"
#include "core/processor.h"
#include <cassert>
#include <iostream>

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config;
    config.id = "obb-test";
    config.storage_dir = ".vfs_storage_obb_test";
    fs::VFSNode vfs(config);

    // 1. Create a subject box: width=10, height=10, depth=10 (Cube)
    fs::Selector box_sel("jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}});
    BoxOp<>::execute(&vfs, box_sel, {-5, 5}, {-5, 5}, {-5, 5});
    Shape box = vfs.read<Shape>(box_sel.with_output("$out"));

    // 2. Non-rotated OBB
    fs::Selector obb1_sel("jot/orientedBoundingBox", {{"$in", box_sel.with_output("$out")}, {"grow", 0.0}});
    OrientedBoundingBoxOp<>::execute(&vfs, obb1_sel, box, 0.0);
    Shape obb1 = vfs.read<Shape>(obb1_sel.with_output("$out"));
    Geometry geo1 = vfs.read<Geometry>(*obb1.geometry);
    CGAL::Bbox_3 b1 = geo1.bounds();
    std::cout << "Non-rotated OBB: " << (b1.xmax()-b1.xmin()) << " x " << (b1.ymax()-b1.ymin()) << " x " << (b1.zmax()-b1.zmin()) << std::endl;
    assert(std::abs(CGAL::to_double(b1.xmax()-b1.xmin()) - 10.0) < 1e-4);

    // 3. Rotate it by 30 degrees (30/360 turns) around all axes
    fs::Selector rot_sel("jot/rotate", {{"$in", box_sel.with_output("$out")}, {"turns", json::array({0.1})}});
    // We'll just rotate around Z for simplicity in verifying dimensions
    RotateZOp<>::execute(&vfs, rot_sel, box, {0.1});
    Shape rotated_box = vfs.read<Shape>(rot_sel.with_output("$out"));

    // 4. Calculate OBB
    fs::Selector obb_sel("jot/orientedBoundingBox", {{"$in", rot_sel.with_output("$out")}, {"grow", 0.0}});
    OrientedBoundingBoxOp<>::execute(&vfs, obb_sel, rotated_box, 0.0);
    Shape obb_shape = vfs.read<Shape>(obb_sel.with_output("$out"));

    // Verify dimensions
    Geometry obb_geo = vfs.read<Geometry>(*obb_shape.geometry);
    CGAL::Bbox_3 gbb = obb_geo.bounds();

    double w = CGAL::to_double(gbb.xmax() - gbb.xmin());
    double d = CGAL::to_double(gbb.ymax() - gbb.ymin());
    double h = CGAL::to_double(gbb.zmax() - gbb.zmin());

    std::cout << "Rotated OBB Dimensions: " << w << " x " << d << " x " << h << std::endl;
    
    // CGAL's OBB is approximate. For a 10x10x10 cube, it should be close to 10x10x10.
    assert(std::abs(w - 10.0) < 0.5);
    assert(std::abs(d - 10.0) < 0.5);
    assert(std::abs(h - 10.0) < 0.5);

    std::cout << "OBB Test Passed!" << std::endl;

    return 0;
}
