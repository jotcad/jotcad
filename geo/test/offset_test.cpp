#include "test_base.h"
#include "../hexagon_op.h"
#include "../offset_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Offset Operation..." << std::endl;
    
    fs::Selector hex_fulfilling = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, hex_fulfilling, 30.0);
    Shape hex = vfs.read<Shape>(hex_fulfilling);
    
    fs::Selector offset_fulfilling = {"jot/offset", {{"diameter", 5.0}}};
    OffsetOp<>::execute(&vfs, offset_fulfilling, hex, 5.0);
    
    Shape s = vfs.read<Shape>(offset_fulfilling);
    vfs.verify_render(s, "offset_op_basic", "3f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f472c");

    std::cout << "✅ Offset PASS" << std::endl;
    return 0;
}
