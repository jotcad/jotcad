#include "test_base.h"
#include "../hexagon_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Hexagon Primitive..." << std::endl;
    MockVFS vfs;
    Shape out;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, out);
    
    assert(out.geometry.has_value());
    assert(out.geometry->path == "geo/mesh");
    assert(out.tags["type"] == "hexagon");
    
    Geometry geo = vfs.read_geo(out.geometry);
    assert(geo.vertices.size() == 6);
    assert(geo.faces.size() == 1);
    
    std::cout << "✅ Hexagon PASS" << std::endl;
    return 0;
}
