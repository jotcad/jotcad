#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include <CGAL/Polygon_mesh_processing/measure.h>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("dilate");
    register_all_ops(&vfs);

    std::cout << "Testing Dilate Operator (Convex Dilation)..." << std::endl;

    // 1. Create a 10x10x10 Box
    fs::Selector box_sel;
    box_sel.path = "jot/Box";
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    box_sel = box_sel.with_output("$out");
    Shape box = vfs.read<Shape>(box_sel);

    // Verify original box is convex and has volume ~1000
    {
        Geometry geo = vfs.read<Geometry>(box.geometry.value());
        boolean::Surface_mesh mesh = boolean::Engine::geometry_to_mesh(geo);
        if (!::CGAL::is_strongly_convex_3(mesh)) {
            std::cerr << "  ❌ FAIL: Original box is not strongly convex." << std::endl;
            return 1;
        }
        double vol = ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(mesh));
        std::cout << "  - Original box volume: " << vol << std::endl;
        if (std::abs(vol - 1000.0) > 1e-5) {
            std::cerr << "  ❌ FAIL: Expected volume 1000, got: " << vol << std::endl;
            return 1;
        }
    }

    // 2. Perform dilation by diameter 4.0 (shifts face planes outwards by 2.0)
    fs::Selector dilate_sel;
    dilate_sel.path = "jot/dilate";
    dilate_sel.parameters["$in"] = box.to_json();
    dilate_sel.parameters["diameter"] = 4.0;
    dilate_sel = dilate_sel.with_output("$out");

    std::cout << "  - Executing jot/dilate with diameter 4.0..." << std::endl;
    Shape result = vfs.read<Shape>(dilate_sel);

    // 3. Verify Dilated Result
    if (!result.geometry.has_value()) {
        std::cerr << "  ❌ FAIL: Dilated result shape has no geometry." << std::endl;
        return 1;
    }

    Geometry dil_geo = vfs.read<Geometry>(result.geometry.value());
    boolean::Surface_mesh dil_mesh = boolean::Engine::geometry_to_mesh(dil_geo);

    // Verify well-formed solid
    vfs.verify_well_formed_solid(dil_geo, "Dilated Box");

    // Verify it is strongly convex
    if (!::CGAL::is_strongly_convex_3(dil_mesh)) {
        std::cerr << "  ❌ FAIL: Dilated box is not strongly convex." << std::endl;
        return 1;
    }
    std::cout << "  - Verified dilated box is strongly convex." << std::endl;

    // Verify expanded volume (should be 14 * 14 * 14 = 2744)
    double dil_vol = ::CGAL::to_double(::CGAL::Polygon_mesh_processing::volume(dil_mesh));
    std::cout << "  - Dilated box volume: " << dil_vol << std::endl;
    if (std::abs(dil_vol - 2744.0) > 1e-2) {
        std::cerr << "  ❌ FAIL: Expected volume 2744, got: " << dil_vol << std::endl;
        return 1;
    }

    std::cout << "  ✅ Dilate Operator Test Passed." << std::endl;
    return 0;
}
