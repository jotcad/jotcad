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
    OrbOp<>::execute(&vfs, orb_sel, Interval{-5.0, 5.0}, Interval{0, 0}, Interval{0, 0}, Interval{0, 0}, 0.1);
    Shape orb = vfs.read<Shape>(orb_sel);
    Geometry o_geo = vfs.read<Geometry>(orb.geometry.value());
    
    std::cout << "  - Sphere vertices: " << o_geo.vertices.size() << std::endl;
    vfs.verify_well_formed_solid(o_geo, "Sphere (d=10)");

    // 2. Ellipsoid (20x10x5 override)
    Selector e_sel = Selector{"jot/Orb", {{"width", 20.0}, {"height", 10.0}, {"depth", 5.0}}}.with_output("$out");
    OrbOp<>::execute(&vfs, e_sel, Interval{-5.0, 5.0}, Interval{-10.0, 10.0}, Interval{-5.0, 5.0}, Interval{-2.5, 2.5}, 0.1);
    Shape ell = vfs.read<Shape>(e_sel);
    Geometry e_geo = vfs.read<Geometry>(ell.geometry.value());
    
    std::cout << "  - Ellipsoid (20x10x5) vertices: " << e_geo.vertices.size() << std::endl;
    vfs.verify_well_formed_solid(e_geo, "Ellipsoid (20x10x5)");

    std::cout << "✅ Orb PASS" << std::endl;
    return 0;
}
