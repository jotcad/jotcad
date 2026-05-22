#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("clip_surface_solid");
    register_all_ops(&vfs);
    
    std::cout << "Testing Clip (Surface by Solid)..." << std::endl;
    
    // 1. Create a 100x100 Box (Surface)
    Selector box_addr = Selector{"jot/Box", {{"width", 100.0}, {"height", 100.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);

    // 2. Create an Orb (Solid) with diameter 50
    Selector orb_addr = Selector{"jot/Orb", {{"diameter", 50.0}}}.with_output("$out");
    Processor::execute(&vfs, orb_addr);

    // 3. Perform Clip: Box clipped by Orb
    Selector clip_addr = Selector{"jot/clip", {{"$in", box_addr}, {"tools", json::array({orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, clip_addr);
    
    Shape out = vfs.read<Shape>(clip_addr);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Resulting geometry has " << res.vertices.size() << " vertices." << std::endl;
    if (res.vertices.size() > 4) {
        std::cout << "  ✅ Vertex count increased (suggests a clip was made)." << std::endl;
    } else {
        std::cerr << "  ❌ Vertex count is still 4 (no clip occurred)." << std::endl;
        return 1;
    }

    // Generate PNG
    Selector png_addr = Selector{"jot/png", {{"$in", clip_addr}}}.with_output("$out");
    Processor::execute(&vfs, png_addr);
    std::vector<uint8_t> png_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
    
    std::filesystem::create_directories("actual");
    std::ofstream out_file("actual/clip_surface_solid.png", std::ios::binary);
    out_file.write((char*)png_bytes.data(), png_bytes.size());
    out_file.close();
    std::cout << "  - Saved image to actual/clip_surface_solid.png" << std::endl;

    std::cout << "Testing Clip (Surface by Solid, Partially Overlapping)..." << std::endl;
    // Offset the orb by 50 in X
    Selector move_orb_addr = Selector{"jot/mx", {{"$in", orb_addr}, {"x", json::array({50.0})}}}.with_output("$out");
    Processor::execute(&vfs, move_orb_addr);

    Selector clip_partial_addr = Selector{"jot/clip", {{"$in", box_addr}, {"tools", json::array({move_orb_addr})}}}.with_output("$out");
    Processor::execute(&vfs, clip_partial_addr);

    Shape out_2 = vfs.read<Shape>(clip_partial_addr);
    Geometry res_2 = vfs.read<Geometry>(out_2.geometry.value());

    std::cout << "  Resulting geometry has " << res_2.vertices.size() << " vertices." << std::endl;
    if (res_2.vertices.size() > 0) {
        std::cout << "  ✅ Result has geometry." << std::endl;
    } else {
        std::cerr << "  ❌ Result is EMPTY." << std::endl;
        return 1;
    }

    // Generate PNG for partial clip
    Selector png_partial_addr = Selector{"jot/png", {{"$in", clip_partial_addr}}}.with_output("$out");
    Processor::execute(&vfs, png_partial_addr);
    std::vector<uint8_t> png_partial_bytes = vfs.read<std::vector<uint8_t>>(png_partial_addr);
    
    std::ofstream out_file_2("actual/clip_surface_solid_partial.png", std::ios::binary);
    out_file_2.write((char*)png_partial_bytes.data(), png_partial_bytes.size());
    out_file_2.close();
    std::cout << "  - Saved image to actual/clip_surface_solid_partial.png" << std::endl;

    std::cout << "✅ Surface by Solid Clip Test Finished" << std::endl;
    return 0;
}
