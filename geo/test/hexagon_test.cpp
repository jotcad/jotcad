#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("hexagon");
    register_all_ops(&vfs);
    
    std::cout << "Testing Refactored Hexagon Primitives..." << std::endl;
    
    // 1. By Diameter (Default orientation)
    fs::Selector hex_dia = fs::Selector{"jot/Hexagon/diameter", {{"diameter", 30.0}, {"turns", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_dia);
    Shape s_dia = vfs.read<Shape>(hex_dia);
    vfs.verify_render(s_dia, "hexagon_op_diameter", "00b6e95084351597deb65f2ca48b9260383500f6cc8c3dffa42febd935d6f656");

    // 2. By Radius
    fs::Selector hex_rad = fs::Selector{"jot/Hexagon/radius", {{"radius", 15.0}, {"turns", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_rad);
    Shape s_rad = vfs.read<Shape>(hex_rad);
    vfs.verify_render(s_rad, "hexagon_op_radius", "00b6e95084351597deb65f2ca48b9260383500f6cc8c3dffa42febd935d6f656");

    // 3. By EdgeLength
    fs::Selector hex_edge = fs::Selector{"jot/Hexagon/edgeLength", {{"edgeLength", 15.0}, {"turns", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_edge);
    Shape s_edge = vfs.read<Shape>(hex_edge);
    vfs.verify_render(s_edge, "hexagon_op_edgeLength", "00b6e95084351597deb65f2ca48b9260383500f6cc8c3dffa42febd935d6f656");

    // 4. With Turns (Pointy-topped rotation: 30 degrees = 1/12 turns)
    fs::Selector hex_pointy = fs::Selector{"jot/Hexagon/diameter", {{"diameter", 30.0}, {"turns", 1.0/12.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_pointy);
    Shape s_pointy = vfs.read<Shape>(hex_pointy);
    vfs.verify_render(s_pointy, "hexagon_op_pointy", "62d53066d0b894ad856e34070e48151a06774e8af12bcbf107b507dd57187a47");

    std::cout << "✅ ALL Hexagon Refactoring Tests Passed" << std::endl;
    return 0;
}
