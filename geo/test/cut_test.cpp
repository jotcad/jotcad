#include "test_base.h"
#include "../box_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing 2D Boolean Cut..." << std::endl;
    MockVFS vfs;
    register_all_ops();

    for (auto const& [path, op] : Processor::registry()) {
        vfs.register_op(path, [&vfs, path](const VFSNode::VFSRequest& req) {
            return Processor::registry()[path].logic(&vfs, req.path, req.parameters, req.stack);
        }, op.schema);
    }
    
    // 1. Create a large box (100x100)
    Shape base;
    BoxOp<>::execute(&vfs, {100.0}, {100.0}, {0.0}, base);
    
    // 2. Create a smaller box (20x20) in the middle
    Shape hole;
    BoxOp<>::execute(&vfs, {20.0}, {20.0}, {0.0}, hole);
    // Keep it at origin (center of base)
    hole.tf = Matrix::identity().to_vec();
    
    // 3. Subtract
    Shape result;
    CutOp<>::execute(&vfs, base, {hole}, result);
    
    // 4. Verify
    Geometry geo = vfs.read_geo(result.geometry);
    
    // In a 2D cut of a square from a square:
    assert(geo.faces.size() == 1);
    assert(geo.faces[0].loops.size() == 2);
    assert(geo.vertices.size() == 8); // 4 + 4
    
    std::cout << "✅ Cut PASS" << std::endl;
    return 0;
}
