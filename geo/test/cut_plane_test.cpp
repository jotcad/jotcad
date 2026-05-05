#include "test_base.h"
#include "../box_op.h"
#include "../plane_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut_plane");
    
    std::cout << "Testing Cut (Mesh by Infinite Plane)..." << std::endl;
    
    // 1. Create a 20x20x20 Cube centered at origin
    // Bounds: [-10, 10]
    fs::Selector box_sel = {"jot/Box", {{"width", 20.0}, {"height", 20.0}, {"depth", 20.0}}};
    BoxOp<>::execute(&vfs, box_sel, Interval{-10.0, 10.0}, Interval{-10.0, 10.0}, Interval{-10.0, 10.0});
    Shape box_shape = vfs.read<Shape>(box_sel);

    // 2. Create a Z Plane moved up by 5 units
    // This plane has normal +Z and passes through (0,0,5).
    // Everything above z=5 should be removed.
    fs::Selector plane_sel = {"jot/Z", {}};
    ZOp<>::execute(&vfs, plane_sel);
    Shape plane_shape = vfs.read<Shape>(plane_sel);
    plane_shape.tf = Matrix::translate(0, 0, 5).to_vec();
    vfs.write(plane_sel, plane_shape);
    
    // 3. Perform Cut
    fs::Selector cut_sel = {"jot/cut/plane", {}};
    CutOp<>::execute(&vfs, cut_sel, box_shape, {plane_shape}, false);
    
    Shape out = vfs.read<Shape>(cut_sel);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Resulting mesh has " << res.vertices.size() << " vertices." << std::endl;

    // The box was [-10, 10] in Z. After cutting by a plane at Z=5 (removing Z > 5),
    // the box should be [-10, 5] in Z.
    double max_z = -1e9;
    for (const auto& v : res.vertices) {
        max_z = (std::max)(max_z, CGAL::to_double(v.z));
    }
    std::cout << "  Max Z coordinate: " << max_z << std::endl;

    assert(std::abs(max_z - 5.0) < 1e-6);

    // Verify it is still a solid
    boolean::Surface_mesh result_mesh = CutOp<>::geometry_to_mesh(res);
    assert(fix::is_geometry_solid(result_mesh));
    std::cout << "  ✅ Result is SOLID." << std::endl;

    std::cout << "✅ Plane Cut PASS" << std::endl;
    return 0;
}
