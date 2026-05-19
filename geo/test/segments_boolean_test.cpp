#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("segments_boolean");
    register_all_ops(&vfs);
    
    // 1. Create a segment from (-100, 0, 0) to (100, 0, 0)
    std::vector<std::vector<double>> pts_data = {{-100.0, 0.0, 0.0}, {100.0, 0.0, 0.0}};
    Selector pts_addr = Selector{"jot/Points", {{"points", pts_data}}}.with_output("$out");
    Processor::execute(&vfs, pts_addr);

    Selector link_addr = Selector{"jot/link", {{"$in", pts_addr}}}.with_output("$out");
    Processor::execute(&vfs, link_addr);

    // 2. Create a Solid Tool: Orb(50) at center
    Selector orb_addr = Selector{"jot/Orb", {{"diameter", 50.0}}}.with_output("$out");
    Processor::execute(&vfs, orb_addr);

    // 3. Create a Surface Tool: Box(50, 50) at X=50
    Selector box_surface_addr = Selector{"jot/Box", {{"width", 50.0}, {"height", 50.0}}}.with_output("$out");
    Processor::execute(&vfs, box_surface_addr);
    Selector move_box_addr = Selector{"jot/mx", {{"$in", box_surface_addr}, {"x", json::array({50.0})}}}.with_output("$out");
    Processor::execute(&vfs, move_box_addr);

    auto save_png = [&](const Selector& addr, const std::string& filename) {
        Selector png_addr = Selector{"jot/png", {{"$in", addr}}}.with_output("$out");
        Processor::execute(&vfs, png_addr);
        std::vector<uint8_t> bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::ofstream out("actual/" + filename, std::ios::binary);
        out.write((char*)bytes.data(), bytes.size());
    };

    // --- TEST CASES ---

    // A. Cut Segments by Solid Orb
    std::cout << "Testing Cut (Segments by Solid)..." << std::endl;
    Selector cut_solid_addr = Selector{"jot/cut", {{"$in", link_addr}, {"tools", json::array({orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, cut_solid_addr);
    save_png(cut_solid_addr, "cut_segments_solid.png");
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_solid_addr).geometry.value());
        std::cout << "  Segments: " << res.segments.size() << std::endl;
        assert(res.segments.size() == 2); // Split into two outer parts
    }

    // B. Clip Segments by Solid Orb
    std::cout << "Testing Clip (Segments by Solid)..." << std::endl;
    Selector clip_solid_addr = Selector{"jot/clip", {{"$in", link_addr}, {"tools", json::array({orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, clip_solid_addr);
    save_png(clip_solid_addr, "clip_segments_solid.png");
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(clip_solid_addr).geometry.value());
        std::cout << "  Segments: " << res.segments.size() << std::endl;
        assert(res.segments.size() == 1); // Only the middle part inside the orb
    }

    // C. Cut Segments by Surface Box
    std::cout << "Testing Cut (Segments by Surface)..." << std::endl;
    Selector cut_surface_addr = Selector{"jot/cut", {{"$in", link_addr}, {"tools", json::array({move_box_addr})}}}.with_output("$out");
    Processor::execute(&vfs, cut_surface_addr);
    save_png(cut_surface_addr, "cut_segments_surface.png");
    {
        Geometry res = vfs.read<Geometry>(vfs.read<Shape>(cut_surface_addr).geometry.value());
        std::cout << "  Segments: " << res.segments.size() << std::endl;
        // Cutting by a surface should ideally split the segment if it passes through the surface boundary
        // or just return the original if it doesn't intersect properly in 3D.
        // In 3D, a segment intersecting a 2D surface is a single point. 
        // Our 'cut_segments_by_mesh' uses AABB tree and splits at intersections.
        // If it's a surface (not closed), 'is_inside' is only true if 'is_on_surface' is true.
        // So it should split at the point of intersection.
    }

    // D. Clip Segments by Surface Box
    std::cout << "Testing Clip (Segments by Surface)..." << std::endl;
    Selector clip_surface_addr = Selector{"jot/clip", {{"$in", link_addr}, {"tools", json::array({move_box_addr})}}}.with_output("$out");
    Processor::execute(&vfs, clip_surface_addr);
    save_png(clip_surface_addr, "clip_segments_surface.png");

    std::cout << "✅ Segments Boolean Test Finished" << std::endl;
    return 0;
}
