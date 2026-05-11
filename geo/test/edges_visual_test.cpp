#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("edges_visual");
    register_all_ops(&vfs);

    std::cout << "Testing Edges Visual (PNG) Validation..." << std::endl;

    // 1. Setup Input: A Box
    Selector box_addr = Selector{"jot/Box", {{"width", 20.0}, {"height", 10.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);

    // 2. Test 'eachEdge' (Component distribution style)
    // 2. Test 'edges' (Component distribution style)
    std::cout << "  - Testing 'edges'..." << std::endl;
    Selector each_addr = Selector{"jot/edges", {{"$in", box_addr}, {"proxy", true}}}.with_output("$out");

    Shape each_out = vfs.read<Shape>(each_addr);
    if (each_out.components.size() != 4) {
        std::cerr << "    ❌ FAIL: edges expected 4 components for a box, got " << each_out.components.size() << std::endl;
        return 1;
    }

    // 3. Render PNG
    Selector png_each = Selector{"jot/png", {{"$in", each_addr}, {"ax", 0.5}, {"ay", 0.5}}}.with_output("$out");
    Processor::execute(&vfs, png_each);

    std::vector<uint8_t> png2 = vfs.read<std::vector<uint8_t>>(png_each.with_output("file"));
    std::filesystem::create_directories("actual");
    std::ofstream out2("actual/edges_test.png", std::ios::binary);
    out2.write((char*)png2.data(), png2.size());
    out2.close();
    std::cout << "    ✅ Saved actual/edges_test.png" << std::endl;

    std::cout << "✨ Edges Visual PASS" << std::endl;
    return 0;
}
