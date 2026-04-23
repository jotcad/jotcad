#include "test_base.h"
#include "../box_op.h"
#include "../points_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Cut (Points by Mesh)..." << std::endl;
    
    // 1. Create a Point Cloud
    std::vector<std::vector<double>> pts_data = {
        {0.0, 0.0, 0.0},
        {5.0, 5.0, 5.0},
        {20.0, 20.0, 20.0}
    };
    
    // In our C++ API, we pass the raw types directly to the execute function.
    // The execute function internally handles P::make_shape and vfs->write.
    
    fs::Selector pts_sel = {"jot/Points", {{"points", pts_data}}};
    PointsOp<>::execute(&vfs, pts_sel, pts_data);
    Shape points_shape = vfs.read<Shape>(pts_sel);

    // 2. Create a Tool Box (10.1x10.1x10.1) centered at (0,0,0)
    fs::Selector tool_sel = {"jot/Box", {{"width", 10.1}, {"height", 10.1}, {"depth", 10.1}}};
    BoxOp<>::execute(&vfs, tool_sel, 10.1, 10.1, 10.1);
    Shape tool_shape = vfs.read<Shape>(tool_sel);
    
    // 3. Perform Cut
    fs::Selector cut_sel = {"jot/cut/points", {}};
    // Re-read shapes from VFS to ensure they are fully populated CIDs
    points_shape = vfs.read<Shape>(pts_sel);
    tool_shape = vfs.read<Shape>(tool_sel);

    CutOp<>::execute(&vfs, cut_sel, points_shape, {tool_shape}, false);
    
    Shape out = vfs.read<Shape>(cut_sel);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Vertices remaining: " << res.vertices.size() << std::endl;
    for (const auto& v : res.vertices) {
        std::cout << "    Point: (" << CGAL::to_double(v.x) << ", " << CGAL::to_double(v.y) << ", " << CGAL::to_double(v.z) << ")" << std::endl;
    }

    // Expecting only (20,20,20) to remain
    assert(res.vertices.size() == 1);
    assert(std::abs(CGAL::to_double(res.vertices[0].x) - 20.0) < 1e-6);

    std::cout << "✅ Points Cut PASS" << std::endl;
    return 0;
}
