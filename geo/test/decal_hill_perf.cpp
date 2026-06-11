#include "test_base.h"
#include <chrono>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("decal_hill_perf");
    register_all_ops(&vfs);

    std::cout << "Starting Decal Hill Performance Test..." << std::endl;

    // 1. Load local heightmap
    std::string img_path = "data/Trencrom_Hill.png";
    std::cout << "  - Loading local image: " << img_path << std::endl;
    
    std::ifstream img_file(img_path, std::ios::binary | std::ios::ate);
    if (!img_file.is_open()) {
        std::cerr << "❌ Failed to open local image: " << img_path << std::endl;
        return 1;
    }
    
    std::streamsize size = img_file.tellg();
    img_file.seekg(0, std::ios::beg);
    std::vector<uint8_t> png_bytes(size);
    if (!img_file.read((char*)png_bytes.data(), size)) {
        std::cerr << "❌ Failed to read local image: " << img_path << std::endl;
        return 1;
    }

    Selector real_img_addr = Selector{"jot/Image", {{"url", "mock://Trencrom_Hill.png"}}}.with_output("$out");
    vfs.write(real_img_addr, png_bytes);

    // 2. Create Relief (High resolution)
    // We'll use a large 300x300 relief to ensure coverage of the orb.
    Selector relief_sel = Selector{"jot/relief", {
        {"$in", real_img_addr.to_json()},
        {"width", 300.0},
        {"breadth", 300.0},
        {"height", 10.0},
        {"base", 0.0},
        {"close", false}
    }}.with_output("$out");
    
    std::cout << "  - Executing ReliefOp..." << std::endl;
    Processor::execute(&vfs, relief_sel);

    // 3. Create Subject: High-res Orb (Sphere)
    // zag=2.0 on a 30mm orb to keep it manageable but testing performance
    Selector orb_sel = Selector{"jot/Orb", {{"diameter", 30.0}, {"zag", 2.0}}}.with_output("$out");
    std::cout << "  - Executing OrbOp (Subject)..." << std::endl;
    Processor::execute(&vfs, orb_sel);

    // 4. Execute Decal (pattern is removed)
    Selector decal_sel = Selector{"jot/decal", {
        {"$in", orb_sel.to_json()},
        {"relief", relief_sel.to_json()},
        {"seam", "skirting"}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (Hill onto Orb)..." << std::endl;
    auto t_start = std::chrono::high_resolution_clock::now();
    try {
        Processor::execute(&vfs, decal_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ DecalOp failed: " << e.what() << std::endl;
        return 1;
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    std::cout << "    * Completed in " << std::chrono::duration<double>(t_end - t_start).count() << "s" << std::endl;

    std::cout << "✅ Decal Hill Performance Test PASS" << std::endl;
    return 0;
}
