#include "test_base.h"
#include "ops/approximate_op.h"
#include "ops/orb_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("approximate_test");
    orb_init(&vfs);
    approximate_init(&vfs);

    std::cout << "Testing Approximate operator (VSA shape approximation)..." << std::endl;

    // 1. Create a sphere (Orb)
    fs::Selector orb_sel("jot/Orb");
    Shape orb = vfs.read<Shape>(orb_sel.with_output("$out"));

    // 2. Approximate with 20 proxies (facets)
    fs::Selector approx_sel("jot/approximate");
    approx_sel.parameters["$in"] = vfs.materialize(orb).value;
    approx_sel.parameters["max_proxies"] = 20;

    std::cout << "  - Approximating (20 proxies)..." << std::endl;
    Shape res = vfs.read<Shape>(approx_sel.with_output("$out"));

    assert(res.geometry.has_value());
    Geometry geo = vfs.read<Geometry>(res.geometry.value());
    std::cout << "  - Resulting triangles: " << geo.faces.size() << std::endl;

    // 3. Verify integrity
    vfs.verify_well_formed_solid(geo, "Approximated Sphere");

    // The sphere starts at 1024 triangles. VSA with 20 proxies generates exactly 60 triangles.
    assert(geo.faces.size() == 60);

    std::cout << "✅ Approximate Test Passed" << std::endl;

    return 0;
}
