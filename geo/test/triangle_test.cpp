#include "test_base.h"
#include "../triangle_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Triangle Primitive..." << std::endl;
    MockVFS vfs;

    Shape out;
    EquilateralTriangleOp<>::execute(&vfs, {10.0}, out);

    assert(out.geometry.has_value());
    Geometry geo = vfs.read_geo(out.geometry);

    // Equilateral triangle: 3 vertices, 1 face with 1 loop of 3 indices
    assert(geo.vertices.size() == 3);
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 1);
    assert(geo.faces[0].loops[0].size() == 3);

    std::cout << "✅ Triangle PASS" << std::endl;
    return 0;
}
