#include "test_base.h"
#include <fstream>

using namespace jotcad::geo;

void render_to(fs::VFSNode* vfs, const fs::Selector& sel, const std::string& name) {
    fs::Selector png_addr = fs::Selector{"jot/png", {{"$in", sel}}}.with_output("$out");
    Processor::execute(vfs, png_addr);
    std::vector<uint8_t> png_bytes = vfs->read<std::vector<uint8_t>>(png_addr.with_output("file"));
    
    if (!png_bytes.empty()) {
        std::filesystem::create_directories("actual");
        std::string filename = "actual/" + name + ".png";
        std::ofstream out(filename, std::ios::binary);
        out.write((char*)png_bytes.data(), png_bytes.size());
        out.close();
        std::cout << "  📸 Rendered " << name << " to " << filename << " (Hash: " << fs::vfs_hash256(png_bytes).substr(0,8) << ")" << std::endl;
    } else {
        std::cerr << "  ⚠️ Failed to render " << name << std::endl;
    }
}

int main() {
    MockVFS vfs("hexagon_at_cut");
    register_all_ops(&vfs);
    
    std::cout << "Testing Hexagon(30).at(eachCorner(), cut(Triangle(2))) Stage-by-Stage..." << std::endl;
    
    // 1. Hexagon(30)
    fs::Selector hex_addr = fs::Selector{"jot/Hexagon/full", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_addr);
    render_to(&vfs, hex_addr, "step1_hexagon");
    
    // 2. eachCorner() frame
    fs::Selector corners_addr = fs::Selector{"jot/eachCorner", {{"$in", hex_addr}}}.with_output("$out");
    Processor::execute(&vfs, corners_addr);
    render_to(&vfs, corners_addr, "step2_corners");

    // 3. Triangle(2)
    fs::Selector tri_addr = fs::Selector{"jot/Triangle/equilateral", {{"size", std::vector<double>{2.0}}}} .with_output("$out");
    Processor::execute(&vfs, tri_addr);
    render_to(&vfs, tri_addr, "step3_triangle");

    // 4. op: cut(Triangle(2))
    // We want to cut the thing being placed ($in) by the Triangle(2).
    fs::Selector cut_template = fs::Selector{"jot/cut", {
        {"tools", std::vector<fs::Selector>{tri_addr}}
    }}.with_output("$out");

    // 5. Hexagon.at(eachCorner(), cut(Triangle(2)))
    fs::Selector at_addr = fs::Selector{"jot/at", {
        {"$in", hex_addr}, 
        {"target", corners_addr},
        {"op", cut_template}
    }}.with_output("$out");

    Processor::execute(&vfs, at_addr);
    render_to(&vfs, at_addr, "step5_final_at");

    return 0;
}
