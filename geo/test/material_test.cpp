#include "test_base.h"
#include "material_op.h"
#include "box_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("material_test");
    box_init(&vfs);
    material_init(&vfs);

    std::cout << "Testing jot/material..." << std::endl;

    // 1. Create a box
    Selector box_sel("jot/Box", {{"width", 10.0}});
    box_sel.output = "$out";
    Shape box = vfs.read<Shape>(box_sel);

    // 2. Apply material
    Selector mat_sel("jot/material", {
        {"$in", vfs.materialize(box).value}, 
        {"material", "wood_grain_01"}
    });
    mat_sel.output = "$out";
    Shape result = vfs.read<Shape>(mat_sel);

    // 3. Verify tag
    if (!result.tags.contains("material")) {
        std::cerr << "FAIL: Result missing 'material' tag." << std::endl;
        return 1;
    }

    if (result.tags["material"] != "wood_grain_01") {
        std::cerr << "FAIL: Incorrect material tag value. Expected 'wood_grain_01', got " << result.tags["material"] << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: Material tag applied correctly." << std::endl;
    return 0;
}
