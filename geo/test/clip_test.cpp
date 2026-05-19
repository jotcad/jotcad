#include "test_base.h"
#include "clip_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("clip_test");
    register_all_ops(&vfs);

    std::cout << "Testing Clip Operation..." << std::endl;

    // 1. 3D Intersection: Two overlapping 10x10x10 boxes
    // Box A: centers at origin (-5,-5,-5) to (5,5,5)
    Selector boxA_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Shape sA = vfs.read<Shape>(boxA_sel);

    // Box B: shifted by (5,5,5) -> centers at (5,5,5), spans (0,0,0) to (10,10,10)
    Shape sB = sA;
    sB.tf = Matrix::translate(FT(5), FT(5), FT(5));
    CID boxB_cid = vfs.materialize(sB);

    Selector clip3d = Selector{"jot/clip", {{"$in", boxA_sel}, {"tools", {boxB_cid}}}}.with_output("$out");
    ClipOp<>::execute(&vfs, clip3d, sA, {sB});
    Shape res3d = vfs.read<Shape>(clip3d);

    assert(res3d.geometry.has_value());
    Geometry geo3d = vfs.read<Geometry>(res3d.geometry.value());
    std::cout << "  - 3D Intersection vertices: " << geo3d.vertices.size() << std::endl;
    // The intersection should be a 5x5x5 box at (0,0,0) to (5,5,5).
    // It should have 8 vertices.
    assert(geo3d.vertices.size() == 8);

    // 2. 2D Intersection: Two overlapping 10x10 rectangles
    Selector rectA_sel = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Shape sRA = vfs.read<Shape>(rectA_sel);
    
    // Rect B: shifted by (5, 5, 0)
    Shape sRB = sRA;
    sRB.tf = Matrix::translate(FT(5), FT(5), FT(0));
    CID rectB_cid = vfs.materialize(sRB);

    Selector clip2d = Selector{"jot/clip", {{"$in", rectA_sel}, {"tools", {rectB_cid}}}}.with_output("$out");
    ClipOp<>::execute(&vfs, clip2d, sRA, {sRB});
    Shape res2d = vfs.read<Shape>(clip2d);

    assert(res2d.geometry.has_value());
    Geometry geo2d = vfs.read<Geometry>(res2d.geometry.value());
    std::cout << "  - 2D Intersection vertices: " << geo2d.vertices.size() << std::endl;
    // Intersection of two 10x10 squares offset by 5 should be a 5x5 square (4 vertices).
    assert(geo2d.vertices.size() == 4);

    // 3. Point Clipping: Cloud of points by a box
    Geometry points_geo;
    points_geo.vertices = {
        {0, 0, 0}, {2, 2, 2}, {10, 10, 10}
    };
    points_geo.points = {0, 1, 2};
    Shape point_cloud;
    point_cloud.geometry = vfs.materialize(points_geo);
    
    // Box tool: 5x5x5 centered at (1,1,1) -> spans (-1.5,-1.5,-1.5) to (3.5,3.5,3.5)
    Selector box_tool_sel = Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 5.0}}}.with_output("$out");
    Shape tool = vfs.read<Shape>(box_tool_sel);
    tool.tf = Matrix::translate(FT(1), FT(1), FT(1));
    
    Selector clip_pts = Selector{"jot/clip", {{"$in", point_cloud}, {"tools", {vfs.materialize(tool)}}}}.with_output("$out");
    ClipOp<>::execute(&vfs, clip_pts, point_cloud, {tool});
    Shape res_pts = vfs.read<Shape>(clip_pts);
    
    assert(res_pts.geometry.has_value());
    Geometry geo_pts = vfs.read<Geometry>(res_pts.geometry.value());
    std::cout << "  - Clipped points: " << geo_pts.vertices.size() << std::endl;
    // (0,0,0) and (2,2,2) are inside the tool. (10,10,10) is outside.
    assert(geo_pts.vertices.size() == 2);

    std::cout << "✅ Clip PASS" << std::endl;
    return 0;
}
