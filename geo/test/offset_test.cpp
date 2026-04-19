#include "test_base.h"
#include "../hexagon_op.h"
#include "../offset_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Rounded Offset..." << std::endl;
    MockVFS vfs;
    
    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    Shape offset_hex;
    OffsetOp<>::execute(&vfs, hex, 5.0, offset_hex);
    
    Geometry geo = vfs.read_geo(offset_hex.geometry);
    // Rounded offset adds many vertices for arcs
    assert(geo.vertices.size() > 6);
    
    std::cout << "✅ Offset PASS" << std::endl;
    return 0;
}
