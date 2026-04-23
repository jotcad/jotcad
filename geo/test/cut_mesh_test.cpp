#include "test_base.h"
#include "../box_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs;
    
    std::cout << "Testing Cut (Mesh by Mesh)..." << std::endl;
    
    // 1. Create Base Cube (20x20x20)
    fs::Selector base_sel = {"jot/Box/base", {{"width", 20.0}, {"height", 20.0}, {"depth", 20.0}}};
    BoxOp<>::execute(&vfs, base_sel, 20.0, 20.0, 20.0);
    Shape base_shape = vfs.read<Shape>(base_sel);

    // 2. Create Tool Cube (10x10x10) centered at (10,10,10)
    // This should overlap one corner of the base cube.
    fs::Selector tool_sel = {"jot/Box/tool", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    BoxOp<>::execute(&vfs, tool_sel, 10.0, 10.0, 10.0);
    Shape tool_shape = vfs.read<Shape>(tool_sel);
    tool_shape.tf = Matrix::translate(10, 10, 10).to_vec();
    vfs.write<Shape>(tool_sel, tool_shape);
    
    // 3. Perform Cut
    fs::Selector cut_sel = {"jot/cut/mesh", {}};
    CutOp<>::execute(&vfs, cut_sel, base_shape, {tool_shape}, false);
    
    Shape out = vfs.read<Shape>(cut_sel);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Resulting mesh has " << res.vertices.size() << " vertices and " << res.faces.size() << " faces." << std::endl;

    // Verify: The base cube had 8 vertices. After a corefinement cut, it should have more.
    assert(res.vertices.size() > 8);
    assert(!res.faces.empty());

    // Defensive check: Is the result unambiguous?
    boolean::Surface_mesh result_mesh = CutOp<>::geometry_to_mesh(res);
    assert(fix::is_geometry_unambiguous(result_mesh));
    std::cout << "  ✅ Result is UNAMBIGUOUS." << std::endl;

    std::cout << "✅ Mesh Cut PASS" << std::endl;
    return 0;
}
