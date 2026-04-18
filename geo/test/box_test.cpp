#include "test_base.h"
#include "../box_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Box Primitive..." << std::endl;
    MockVFS vfs;
    Shape out;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {10.0}, out);
    
    assert(out.geometry.path == "geo/mesh");
    assert(out.tags["type"] == "box");
    
    Geometry geo = vfs.read_geo(out.geometry);
    assert(geo.vertices.size() == 8); // 3D Box
    assert(geo.faces.size() == 6); // 6 faces
    
    std::cout << "✅ Box PASS" << std::endl;
    return 0;
}
