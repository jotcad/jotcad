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
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape box_mold = vfs.read<Shape>(mold_sel);

    assert(box_mold.components.size() >= 2);
    std::cout << "    - Successfully produced mold assembly with " << box_mold.components.size() << " components." << std::endl;

    // 3. Test Pour Prep + Mold on L-Bracket (Concave / Re-entrant Corner)
    std::cout << "  - Testing pourPrep + mold on Concave L-Bracket..." << std::endl;
    fs::Selector l_box_sel("jot/Box");
    l_box_sel.parameters["width"] = 20.0;
    l_box_sel.parameters["height"] = 20.0;
    l_box_sel.parameters["depth"] = 10.0;
    Shape l_base = vfs.read<Shape>(l_box_sel.with_output("$out"));

    fs::Selector l_notch_sel("jot/Box");
    l_notch_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
    l_notch_sel.parameters["height"] = nlohmann::json::array({0.0, 11.0});
    l_notch_sel.parameters["depth"] = nlohmann::json::array({-6.0, 6.0});
    Shape l_notch = vfs.read<Shape>(l_notch_sel.with_output("$out"));

    fs::Selector l_cut_sel("jot/cut");
    l_cut_sel.parameters["$in"] = l_base.to_json();
    l_cut_sel.parameters["tools"] = nlohmann::json::array({l_notch.to_json()});
    Shape l_shape = vfs.read<Shape>(l_cut_sel.with_output("$out"));

    fs::Selector l_pour_sel("jot/pourPrep");
    l_pour_sel.parameters["$in"] = l_shape.to_json();
    l_pour_sel.parameters["sprue_base"] = 6.0;
    l_pour_sel.parameters["sprue_top"] = 12.0;
    l_pour_sel.parameters["vent_dia"] = 2.0;
    l_pour_sel.parameters["auto_orient"] = true;
    l_pour_sel.parameters["vents"] = false;
    l_pour_sel.parameters["min_angle"] = 15.0 / 360.0;
    l_pour_sel.output = "$out";

    Processor::execute(&vfs, l_pour_sel);
    Shape prepped_l = vfs.read<Shape>(l_pour_sel);
    assert(prepped_l.is_real());

    fs::Selector l_mold_sel("jot/mold");
    l_mold_sel.parameters["$in"] = prepped_l.to_json();
    l_mold_sel.parameters["padding"] = 5.0;
    l_mold_sel.parameters["explode"] = 20.0;
    l_mold_sel.parameters["draft"] = 0.0;
    l_mold_sel.output = "$out";

    Processor::execute(&vfs, l_mold_sel);
    Shape l_mold_result = vfs.read<Shape>(l_mold_sel);

    assert(l_mold_result.components.size() >= 3);
    std::cout << "    - Successfully produced L-bracket mold assembly with " << l_mold_result.components.size() << " components." << std::endl;

    std::cout << "✅ ALL Pour Prep Tests Passed" << std::endl;
    return 0;
}
