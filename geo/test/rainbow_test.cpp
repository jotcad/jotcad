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

    // 4. Test jot/color with opaque argument
    std::cout << "  - Testing jot/color with opaque argument..." << std::endl;
    fs::Selector opaque_sel("jot/color");
    opaque_sel.parameters["$in"] = box.to_json();
    opaque_sel.parameters["color"] = "blue";
    opaque_sel.parameters["opaque"] = 0.4;
    Shape opaque_box = vfs.read<Shape>(opaque_sel.with_output("$out"));
    assert(opaque_box.opacity() == 0.4);
    assert(opaque_box.tags["color"].get<std::string>() == "blue");
    std::cout << "    - jot/color applied opacity: " << opaque_box.opacity() << std::endl;

    // 5. Test jot/color with 8-hex alpha string (#6688cc80)
    std::cout << "  - Testing jot/color with 8-hex alpha (#6688cc80)..." << std::endl;
    fs::Selector hex8_sel("jot/color");
    hex8_sel.parameters["$in"] = box.to_json();
    hex8_sel.parameters["color"] = "#6688cc80";
    Shape hex8_box = vfs.read<Shape>(hex8_sel.with_output("$out"));
    assert(std::abs(hex8_box.opacity() - (128.0 / 255.0)) < 1e-3);
    assert(hex8_box.tags["color"].get<std::string>() == "#6688cc");
    std::cout << "    - jot/color parsed hex alpha opacity: " << hex8_box.opacity() << std::endl;

    // 6. Test jot/color with mix factor (blue with 2/3 wash of red)
    std::cout << "  - Testing jot/color with mix factor..." << std::endl;
    fs::Selector blue_sel("jot/color");
    blue_sel.parameters["$in"] = box.to_json();
    blue_sel.parameters["color"] = "blue";
    Shape blue_box = vfs.read<Shape>(blue_sel.with_output("$out"));

    fs::Selector mix_sel("jot/color");
    mix_sel.parameters["$in"] = blue_box.to_json();
    mix_sel.parameters["color"] = "red";
    mix_sel.parameters["mix"] = 2.0 / 3.0;
    Shape mix_box = vfs.read<Shape>(mix_sel.with_output("$out"));
    assert(mix_box.tags.contains("color"));
    std::cout << "    - Blended color: " << mix_box.tags["color"].get<std::string>() << std::endl;

    std::cout << "  ✅ Rainbow and Unified Color (Mix / Opaque / 8-Hex) Test Passed." << std::endl;
    return 0;
}
