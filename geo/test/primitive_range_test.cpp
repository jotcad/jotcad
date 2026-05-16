#include "test_base.h"
#include "../math/interval.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("primitive_range");
    register_all_ops(&vfs);

    std::cout << "Testing Primitives with Range Arguments..." << std::endl;

    // 1. Box with range [30] -> [0, 30]
    {
        Selector sel = Selector{"jot/Box", {
            {"width", json::array({30.0})},
            {"height", 10.0},
            {"depth", 10.0}
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape result = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        bool found_0 = false, found_30 = false;
        for (const auto& v : geo.vertices) {
            if (std::abs(CGAL::to_double(v.x) - 0.0) < 1e-6) found_0 = true;
            if (std::abs(CGAL::to_double(v.x) - 30.0) < 1e-6) found_30 = true;
        }
        if (found_0 && found_30) std::cout << "  ✅ Box([30]) OK" << std::endl;
        else { std::cerr << "  ❌ Box([30]) Failed" << std::endl; return 1; }
    }

    // 2. Disk with range [20, 40]
    {
        // Use a resolution that hits quadrants (4, 8, 12...)
        Selector sel = Selector{"jot/Disk", {
            {"diameter", json::array({20.0, 40.0})},
            {"start", 0.0}, {"end", 1.0},
            {"zag", 0.01} // High resolution to get close to 20
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape result = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(result.geometry.value());

        double min_x = 1e9, max_x = -1e9;
        for (const auto& v : geo.vertices) {
            min_x = std::min(min_x, CGAL::to_double(v.x));
            max_x = std::max(max_x, CGAL::to_double(v.x));
        }
        // With zag 0.01, we should be within 0.01 units
        if (min_x < 20.1 && max_x > 39.9 && min_x >= 20.0 && max_x <= 40.0) {
            std::cout << "  ✅ Disk([20, 40]) OK (Approx range: [" << min_x << ", " << max_x << "])" << std::endl;
        } else {
            std::cerr << "  ❌ Disk([20, 40]) Failed: Got range [" << min_x << ", " << max_x << "]" << std::endl;
            return 1;
        }
    }

    // 3. Orb with asymmetric depth [50]
    {
        Selector sel = Selector{"jot/Orb", {
            {"diameter", 10.0},
            {"depth", json::array({50.0})}
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape result = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(result.geometry.value());

        double min_z = 1e9, max_z = -1e9;
        for (const auto& v : geo.vertices) {
            min_z = std::min(min_z, CGAL::to_double(v.z));
            max_z = std::max(max_z, CGAL::to_double(v.z));
        }
        if (min_z >= 0.0 && max_z <= 50.0 && max_z > 49.0) {
            std::cout << "  ✅ Orb(depth=[50]) OK" << std::endl;
        } else {
            std::cerr << "  ❌ Orb(depth=[50]) Failed: Got range [" << min_z << ", " << max_z << "]" << std::endl;
            return 1;
        }
    }

    std::cout << "✅ All Primitive Range tests passed." << std::endl;
    return 0;
}
