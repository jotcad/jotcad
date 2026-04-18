#include "test_base.h"
#include "../box_op.h"
#include "../outline_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Outline Segments..." << std::endl;
    MockVFS vfs;
    
    Shape box;
    BoxOp<>::execute(&vfs, {10.0}, {10.0}, {10.0}, box);
    
    Shape outline;
    OutlineOp<>::execute(&vfs, box, outline);
    
    Geometry geo = vfs.read_geo(outline.geometry);
    assert(geo.segments.size() > 0);
    assert(geo.triangles.empty());
    
    std::cout << "✅ Outline PASS" << std::endl;
    return 0;
}
