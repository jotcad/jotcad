#include "test_base.h"
#include "protocols.h"
#include "processor.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("pour_test");
    register_all_ops(&vfs);

    std::cout << "Testing jot/pour_prep operator..." << std::endl;

    // 1. Test Pour Prep on Box (Orientation, Single-Peak, and Sprue Funnel)
    std::cout << "  - Testing pourPrep on Box(10, 10, 10)..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    Shape box_shape = vfs.read<Shape>(box_sel.with_output("$out"));

    fs::Selector pour_sel("jot/pourPrep");
    pour_sel.parameters["$in"] = box_shape.to_json();
    pour_sel.parameters["sprue_base"] = 6.0;
    pour_sel.parameters["sprue_top"] = 12.0;
    pour_sel.parameters["vent_dia"] = 2.0;
    pour_sel.parameters["auto_orient"] = true;
    pour_sel.parameters["vents"] = true;
    pour_sel.output = "$out";

    Processor::execute(&vfs, pour_sel);
    Shape prepped_box = vfs.read<Shape>(pour_sel);

    assert(prepped_box.is_real());
    bool has_sprue = false;
    for (const auto& comp : prepped_box.components) {
        if (comp.has_tag("mold/role", "sprue")) has_sprue = true;
    }
    assert(has_sprue);
    std::cout << "    - Confirmed pour_sprue component attached to Box." << std::endl;

    // 2. Test Multi-Piece Mold Decomposition on the Prepped Box with Sprue
    std::cout << "  - Testing mold decomposition on prepped box with sprue..." << std::endl;
    fs::Selector mold_sel("jot/mold");
    mold_sel.parameters["$in"] = prepped_box.to_json();
    mold_sel.parameters["padding"] = 5.0;
    mold_sel.parameters["explode"] = 15.0;
    mold_sel.parameters["draft"] = 0.0;
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape box_mold = vfs.read<Shape>(mold_sel);

    assert(box_mold.components.size() >= 2);
    std::cout << "    - Successfully produced mold assembly with " << box_mold.components.size() << " components." << std::endl;

    std::cout << "✅ ALL Pour Prep Tests Passed" << std::endl;
    return 0;
}
