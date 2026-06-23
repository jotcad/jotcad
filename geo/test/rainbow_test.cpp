#include "test_base.h"
#include "protocols.h"
#include "processor.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("rainbow");
    register_all_ops(&vfs);

    std::cout << "Testing Rainbow Operator..." << std::endl;

    // 1. Create 3 boxes
    fs::Selector box_sel;
    box_sel.path = "jot/Box";
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    box_sel = box_sel.with_output("$out");
    Shape box = vfs.read<Shape>(box_sel);

    Shape g;
    g.components = {box, box, box};
    g.add_tag("type", "group");

    // 2. Perform Rainbow coloring
    fs::Selector rainbow_sel;
    rainbow_sel.path = "jot/rainbow";
    rainbow_sel.parameters["$in"] = g.to_json();
    rainbow_sel = rainbow_sel.with_output("$out");

    std::cout << "  - Executing jot/rainbow..." << std::endl;
    Shape result = vfs.read<Shape>(rainbow_sel);

    // 3. Verify Result
    if (result.components.size() != 3) {
        std::cerr << "  ❌ FAIL: Expected 3 components in group, got: " << result.components.size() << std::endl;
        return 1;
    }

    std::vector<std::string> expected_colors = {"#ee2b2b", "#2bee2b", "#2b2bee"};
    for (size_t i = 0; i < 3; ++i) {
        std::string color = result.components[i].tags.value("color", "");
        if (color != expected_colors[i]) {
            std::cerr << "  ❌ FAIL: Component " << i << " expected color " << expected_colors[i] << ", got: " << color << std::endl;
            return 1;
        } else {
            std::cout << "    - Component " << i << " has correct color: " << color << std::endl;
        }
    }

    std::cout << "  ✅ Rainbow Operator Test Passed." << std::endl;
    return 0;
}
