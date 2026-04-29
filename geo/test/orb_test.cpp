#include "test_base.h"
#include "orb_op.h"
#include "boolean/engine.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("orb_test");
    register_all_ops(&vfs);

    std::cout << "Testing Orb Operation..." << std::endl;

    // 1. Sphere (diameter 10)
    Selector orb_sel = Selector{"jot/Orb", {{"diameter", 10.0}}}.with_output("$out");
    OrbOp<>::execute(&vfs, orb_sel, 10.0, 0, 0, 0, 0.1);
    Shape orb = vfs.read<Shape>(orb_sel);
    Geometry o_geo = vfs.read<Geometry>(orb.geometry.value());
    
    std::cout << "  - Sphere vertices: " << o_geo.vertices.size() << std::endl;
    assert(CGAL::is_closed(boolean::Engine::geometry_to_mesh(o_geo)));

    // 2. Ellipsoid (20x10x5 override)
    Selector e_sel = Selector{"jot/Orb", {{"width", 20.0}, {"height", 10.0}, {"depth", 5.0}}}.with_output("$out");
    OrbOp<>::execute(&vfs, e_sel, 10.0, 20.0, 10.0, 5.0, 0.1);
    Shape ell = vfs.read<Shape>(e_sel);
    Geometry e_geo = vfs.read<Geometry>(ell.geometry.value());
    
    std::cout << "  - Ellipsoid (20x10x5) vertices: " << e_geo.vertices.size() << std::endl;
    assert(CGAL::is_closed(boolean::Engine::geometry_to_mesh(e_geo)));

    std::cout << "✅ Orb PASS" << std::endl;
    return 0;
}
