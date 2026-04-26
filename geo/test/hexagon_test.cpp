#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("hexagon");
    register_all_ops(&vfs);
    
    std::cout << "Testing Hexagon Primitives..." << std::endl;
    
    // 1. Full Hexagon
    fs::Selector hex_addr = fs::Selector{"jot/Hexagon/full", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_addr);
    Shape s_full = vfs.read<Shape>(hex_addr);
    vfs.verify_render(s_full, "hexagon_op_full", "00b6e95084351597deb65f2ca48b9260383500f6cc8c3dffa42febd935d6f656");

    // 2. Hexagon Cap
    fs::Selector cap_addr = fs::Selector{"jot/Hexagon/cap", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, cap_addr);
    Shape s_cap = vfs.read<Shape>(cap_addr);
    vfs.verify_render(s_cap, "hexagon_op_cap", "62d53066d0b894ad856e34070e48151a06774e8af12bcbf107b507dd57187a47");

    std::cout << "✅ ALL Hexagon Tests Passed" << std::endl;
    return 0;
}
