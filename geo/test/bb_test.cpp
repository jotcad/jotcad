#include "ops/bb_op.h"
#include "ops/box_op.h"
#include "core/processor.h"
#include <cassert>
#include <iostream>

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config;
    config.id = "bb-test";
    config.storage_dir = ".vfs_storage_bb_test";
    fs::VFSNode vfs(config);

    // 1. Create a subject box: width=10, height=20, depth=30 centered at origin
    // Geometry will be [-5, 5] x [-10, 10] x [-15, 15]
    fs::Selector box_sel("jot/Box", {{"width", 10.0}, {"height", 20.0}, {"depth", 30.0}});
    BoxOp<>::execute(&vfs, box_sel, {-5, 5}, {-10, 10}, {-15, 15});
    
    Shape subject = vfs.read<Shape>(box_sel.with_output("$out"));
    
    // 2. Run BB op on the subject
    fs::Selector bb_sel("jot/boundingBox", {{"$in", box_sel.with_output("$out")}, {"grow", 2.0}});
    BoundingBoxOp<>::execute(&vfs, bb_sel, subject, 2.0);
    
    Shape bb_shape = vfs.read<Shape>(bb_sel.with_output("$out"));
    
    // Verify results
    // Expected bounds: [-5-2, 5+2] x [-10-2, 10+2] x [-15-2, 15+2]
    // = [-7, 7] x [-12, 12] x [-17, 17]
    // Size = [14, 24, 34]
    // tf = translate(-7, -12, -17)
    
    std::cout << "BB TF: " << bb_shape.tf.to_vec() << std::endl;
    
    Matrix expected_tf = Matrix::translate(FT(-7), FT(-12), FT(-17));
    assert(bb_shape.tf == expected_tf);
    
    Geometry bb_geo = vfs.read<Geometry>(*bb_shape.geometry);
    CGAL::Bbox_3 gbb = bb_geo.bounds();
    
    assert(std::abs(CGAL::to_double(gbb.xmin()) - 0.0) < 1e-6);
    assert(std::abs(CGAL::to_double(gbb.xmax()) - 14.0) < 1e-6);
    assert(std::abs(CGAL::to_double(gbb.ymax()) - 24.0) < 1e-6);
    assert(std::abs(CGAL::to_double(gbb.zmax()) - 34.0) < 1e-6);

    std::cout << "BB Test Passed!" << std::endl;

    return 0;
}
