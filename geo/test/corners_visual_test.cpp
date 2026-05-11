#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("corners_visual");
    register_all_ops(&vfs);

    std::cout << "Testing Corners Visual (PNG) Validation..." << std::endl;

    // 1. Setup Input: A Box
    Selector box_addr = Selector{"jot/Box", {{"width", 20.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);

    // 2. Test 'corners' (Point cloud style)
    std::cout << "  - Testing 'corners'..." << std::endl;
    Selector corners_addr = Selector{"jot/corners", {{"$in", box_addr}}}.with_output("$out");
    Processor::execute(&vfs, corners_addr);

    // Operation templates MUST target their output port ($out)
    Selector png_corners = Selector{"jot/png", {{"$in", corners_addr}, {"ax", 0.5}, {"ay", 0.5}}}.with_output("$out");
    Processor::execute(&vfs, png_corners);
    
    std::vector<uint8_t> png1 = vfs.read<std::vector<uint8_t>>(png_corners.with_output("file"));
    std::filesystem::create_directories("actual");
    std::ofstream out1("actual/corners_test.png", std::ios::binary);
    out1.write((char*)png1.data(), png1.size());
    out1.close();
    std::cout << "    ✅ Saved actual/corners_test.png" << std::endl;

    // 3. Test 'eachCorner' (Component distribution style)
    // 3. Test 'corners' (Component distribution style)
    std::cout << "  - Testing 'corners' (components)..." << std::endl;
    Selector each_addr = Selector{"jot/corners", {{"$in", box_addr}, {"proxy", true}}}.with_output("$out");

    Shape each_out = vfs.read<Shape>(each_addr);
    if (each_out.components.size() != 4) {
        std::cerr << "    ❌ FAIL: corners expected 4 components for a box, got " << each_out.components.size() << std::endl;
        return 1;
    }

    // Operation templates MUST target their output port ($out)
    Selector png_each = Selector{"jot/png", {{"$in", each_addr}, {"ax", 0.5}, {"ay", 0.5}}}.with_output("$out");
    Processor::execute(&vfs, png_each);

    std::vector<uint8_t> png2 = vfs.read<std::vector<uint8_t>>(png_each.with_output("file"));
    std::ofstream out2("actual/corners_components_test.png", std::ios::binary);
    out2.write((char*)png2.data(), png2.size());
    out2.close();
    std::cout << "    ✅ Saved actual/corners_components_test.png" << std::endl;

    std::cout << "✨ Corners Visual PASS" << std::endl;
    return 0;
}
