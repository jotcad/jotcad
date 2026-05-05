#include "test_base.h"
#include "../box_op.h"
#include "../points_op.h"
#include "../segment_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Cut (Segments by Mesh)..." << std::endl;
    
    // 1. Create a Line Segment passing through the origin
    // From (-20, 0, 0) to (20, 0, 0)
    std::vector<std::vector<double>> pts_data = {
        {-20.0, 0.0, 0.0},
        {20.0, 0.0, 0.0}
    };
    fs::Selector pts_sel = {"jot/Points", {{"points", pts_data}}};
    PointsOp<>::execute(&vfs, pts_sel, pts_data);
    Shape points_shape = vfs.read<Shape>(pts_sel);

    fs::Selector link_sel = {"jot/link", {{"shapes", {points_shape}}}};
    LinkOp<>::execute(&vfs, link_sel, {points_shape});
    Shape segments_shape = vfs.read<Shape>(link_sel);

    // 2. Create a Tool Box (10x10x10) centered at (0,0,0)
    // This box covers (-5,-5,-5) to (5,5,5).
    // It should clip the segment into two parts: (-20 to -5) and (5 to 20).
    fs::Selector tool_sel = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    BoxOp<>::execute(&vfs, tool_sel, Interval{-5.0, 5.0}, Interval{-5.0, 5.0}, Interval{-5.0, 5.0});
    Shape tool_shape = vfs.read<Shape>(tool_sel);
    
    // 3. Perform Cut
    fs::Selector cut_sel = {"jot/cut/segments", {}};
    CutOp<>::execute(&vfs, cut_sel, segments_shape, {tool_shape}, false);
    
    Shape out = vfs.read<Shape>(cut_sel);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Segments remaining: " << res.segments.size() << std::endl;
    for (const auto& s : res.segments) {
        auto p1 = res.vertices[s[0]];
        auto p2 = res.vertices[s[1]];
        std::cout << "    Segment: (" << CGAL::to_double(p1.x) << ") to (" << CGAL::to_double(p2.x) << ")" << std::endl;
    }

    // Expecting 2 segments: (-20 to -5) and (5 to 20)
    assert(res.segments.size() == 2);
    
    std::cout << "✅ Segments Cut PASS" << std::endl;
    return 0;
}
