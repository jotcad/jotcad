#include "test_base.h"
#include "ops/simplify_op.h"
#include "ops/orb_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("simplify_test");
    orb_init(&vfs);
    simplify_init(&vfs);

    std::cout << "Testing Simplify operator (Sphere reduction)..." << std::endl;

    // 1. Create a sphere (Orb)
    fs::Selector orb_sel("jot/Orb");
    Shape orb = vfs.read<Shape>(orb_sel.with_output("$out"));

    // 2. Simplify with aggressive ratio (1%)
    fs::Selector sim_sel("jot/simplify");
    sim_sel.parameters["$in"] = vfs.materialize(orb).value;
    sim_sel.parameters["ratio"] = 0.01;

    std::cout << "  - Simplifying (1% ratio)..." << std::endl;
    Shape res = vfs.read<Shape>(sim_sel.with_output("$out"));

    assert(res.geometry.has_value());
    Geometry geo = vfs.read<Geometry>(res.geometry.value());
    std::cout << "  - Resulting triangles: " << geo.faces.size() << std::endl;

    // 3. Verify integrity
    vfs.verify_well_formed_solid(geo, "Simplified Sphere");

    // The sphere starts at 1024 triangles. 1% is ~10, but SMS is conservative.
    // 70 triangles is a healthy reduction while maintaining closure.
    assert(geo.faces.size() < 200); 
    assert(geo.faces.size() > 10);

    std::cout << "✅ Simplify Test Passed" << std::endl;

    return 0;
}
