#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("points_boolean");
    register_all_ops(&vfs);
    
    // 1. Create a set of points
    std::vector<std::vector<double>> pts_data = {
        {0.0, 0.0, 0.0},    // Origin
        {10.0, 0.0, 0.0},   // Inside Orb(50)
        {60.0, 0.0, 0.0},   // Outside Orb(50), Inside Box(50) at X=50
        {100.0, 0.0, 0.0}   // Outside everything
    };
    Selector pts_addr = Selector{"jot/Points", {{"points", pts_data}}}.with_output("$out");
    Processor::execute(&vfs, pts_addr);

    // 2. Create a Solid Tool: Orb(50) at center
    Selector orb_addr = Selector{"jot/Orb", {{"diameter", 50.0}}}.with_output("$out");
    Processor::execute(&vfs, orb_addr);

    // 3. Create a Surface Tool: Box(50, 50) at X=60
    Selector box_surface_addr = Selector{"jot/Box", {{"width", 50.0}, {"height", 50.0}}}.with_output("$out");
    Processor::execute(&vfs, box_surface_addr);
    Selector move_box_addr = Selector{"jot/mx", {{"$in", box_surface_addr}, {"x", json::array({60.0})}}}.with_output("$out");
    Processor::execute(&vfs, move_box_addr);

    // --- TEST CASES ---

    // A. Cut Points by Solid Orb
    std::cout << "Testing Cut (Points by Solid)..." << std::endl;
    Selector cut_solid_addr = Selector{"jot/cut", {{"$in", pts_addr}, {"tools", json::array({orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, cut_solid_addr);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_solid_addr).geometry.value());
        std::cout << "  Points remaining: " << res.vertices.size() << std::endl;
        // Points (0,0,0) and (10,0,0) should be removed. (60,0,0) and (100,0,0) remain.
        assert(res.vertices.size() == 2);
    }

    // B. Clip Points by Solid Orb
    std::cout << "Testing Clip (Points by Solid)..." << std::endl;
    Selector clip_solid_addr = Selector{"jot/clip", {{"$in", pts_addr}, {"tools", json::array({orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, clip_solid_addr);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(clip_solid_addr).geometry.value());
        std::cout << "  Points remaining: " << res.vertices.size() << std::endl;
        // Only (0,0,0) and (10,0,0) should remain.
        assert(res.vertices.size() == 2);
    }

    // C. Cut Points by Surface (Expect point on surface to be removed)
    std::cout << "Testing Cut (Points by Surface)..." << std::endl;
    Selector cut_surface_addr = Selector{"jot/cut", {{"$in", pts_addr}, {"tools", json::array({move_box_addr})}}}.with_output("$out");
    Processor::execute(&vfs, cut_surface_addr);
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_surface_addr).geometry.value());
        std::cout << "  Points remaining: " << res.vertices.size() << std::endl;
        // (60,0,0) is on the surface. (0,0,0), (10,0,0), and (100,0,0) are NOT on the surface.
        // So 3 points should remain.
        assert(res.vertices.size() == 3);
    }

    std::cout << "✅ Points Boolean Test Finished" << std::endl;
    return 0;
}
