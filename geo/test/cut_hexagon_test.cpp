#include "test_base.h"
#include "../hexagon_op.h"
#include "../triangle_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Hexagon(30).cut(Triangle(2))..." << std::endl;
    MockVFS vfs;
    register_all_ops();

    for (auto const& [path, op] : Processor::registry()) {
        vfs.register_op(path, [&vfs, path](const VFSNode::VFSRequest& req) {
            return Processor::registry()[path].logic(&vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }
    
    // 1. Create Hexagon(30)
    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    // 2. Create Triangle(2)
    Shape tri;
    EquilateralTriangleOp<>::execute(&vfs, {2.0}, tri);
    
    // 3. Subtract
    Shape result;
    CutOp<>::execute(&vfs, hex, {tri}, result);
    
    // 4. Verify
    Geometry geo = vfs.read_geo(result.geometry);
    
    // It SHOULD have a hole. 1 face, 2 loops.
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 2);
    // Hex(6) + Tri(3) = 9 vertices
    assert(geo.vertices.size() == 9);
    
    std::cout << "✅ Cut Hexagon/Triangle PASS" << std::endl;
    return 0;
}
