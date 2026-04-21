#include "test_base.h"
#include "../hexagon_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Hexagon Primitives..." << std::endl;
    
    // 1. Full Hexagon
    fs::Selector hex_addr = {"jot/Hexagon/full", {{"radius", 10.0}}};
    HexagonFullOp<>::execute(&vfs, hex_addr, 10.0);
    Shape s_full = vfs.read<Shape>(hex_addr);
    vfs.verify_render(s_full, "hexagon_full", "fa7711eb997fd3b9460bab19e44c717c14767b4f26d748505e053f4771c35048");

    // 2. Hexagon Cap
    fs::Selector cap_addr = {"jot/Hexagon/cap", {{"radius", 10.0}}};
    HexagonCapOp<>::execute(&vfs, cap_addr, 10.0);
    Shape s_cap = vfs.read<Shape>(cap_addr);
    vfs.verify_render(s_cap, "hexagon_cap", "");

    std::cout << "✅ ALL Hexagon Tests Passed" << std::endl;
    return 0;
}
