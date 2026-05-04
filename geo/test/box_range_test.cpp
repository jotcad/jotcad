#include "test_base.h"
#include "../math/interval.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("box_range");
    register_all_ops(&vfs);

    std::cout << "Testing Box with Range Argument [30]..." << std::endl;

    // Selector: jot/Box?width=[30]
    Selector sel = Selector{"jot/Box", {
        {"width", json::array({30.0})},
        {"height", 10.0},
        {"depth", 10.0}
    }}.with_output("$out");

    try {
        Processor::execute(&vfs, sel);
        Shape result = vfs.read<Shape>(sel);
        
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        // Verify vertices for asymmetric width [0, 30]
        bool found_0 = false, found_30 = false;
        for (const auto& v : geo.vertices) {
            if (CGAL::to_double(v.x) == 0.0) found_0 = true;
            if (CGAL::to_double(v.x) == 30.0) found_30 = true;
        }

        if (found_0 && found_30) {
            std::cout << "✅ Box with range [30] successful. Vertices mapped correctly to [0, 30]." << std::endl;
        } else {
            std::cerr << "❌ Box vertices incorrect. Min/Max X not found." << std::endl;
            for (const auto& v : geo.vertices) {
                std::cout << "  - V: " << CGAL::to_double(v.x) << ", " << CGAL::to_double(v.y) << ", " << CGAL::to_double(v.z) << std::endl;
            }
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "❌ Box with [30] failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
