#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "infra/stl.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("mold_test");
    register_all_ops(&vfs);

    std::cout << "Testing jot/mold operator..." << std::endl;

    // 1. Test 2-piece mold on convex box
    std::cout << "  - Testing 2-piece mold on Box..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    Shape box_shape = vfs.read<Shape>(box_sel.with_output("$out"));

    fs::Selector mold_sel("jot/mold");
    mold_sel.parameters["$in"] = box_shape.to_json();
    mold_sel.parameters["padding"] = 5.0;
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape box_mold = vfs.read<Shape>(mold_sel);

    assert(box_mold.tags["pieces"].get<int>() == 2);
    assert(box_mold.components.size() == 3); // 2 mold blocks + 1 original shape
    assert(box_mold.components[0].tags.contains("mold") && box_mold.components[0].tags["mold"] == 1);
    assert(box_mold.components[1].tags.contains("mold") && box_mold.components[1].tags["mold"] == 2);
    std::cout << "    - Produced 2-piece mold blocks + original shape successfully." << std::endl;

    // 2. Test 3-piece mold on bear.stl with explode
    std::cout << "  - Testing 3-piece mold on bear.stl with explode=15.0..." << std::endl;
    Geometry bear_geo;
    bool success = STLReader::read_file("../../scratch/bear.stl", bear_geo);
    if (!success) {
        success = STLReader::read_file("../../../scratch/bear.stl", bear_geo);
    }
    assert(success);

    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, {{"type", "closed"}});
    fs::Selector bear_mold_sel("jot/mold");
    bear_mold_sel.parameters["$in"] = bear_shape.to_json();
    bear_mold_sel.parameters["padding"] = 10.0;
    bear_mold_sel.parameters["explode"] = 15.0;
    bear_mold_sel.output = "$out";

    Processor::execute(&vfs, bear_mold_sel);
    Shape bear_mold = vfs.read<Shape>(bear_mold_sel);

    assert(bear_mold.tags["pieces"].get<int>() == 3);
    assert(bear_mold.components.size() == 4); // 3 mold blocks + 1 original shape
    assert(bear_mold.components[0].tags.contains("mold") && bear_mold.components[0].tags["mold"] == 1);
    assert(bear_mold.components[1].tags.contains("mold") && bear_mold.components[1].tags["mold"] == 2);
    assert(bear_mold.components[2].tags.contains("mold") && bear_mold.components[2].tags["mold"] == 3);
    std::cout << "    - Produced 3-piece mold blocks + original shape successfully with side insert." << std::endl;
    std::cout << "  ✅ jot/mold test passed." << std::endl;
    return 0;
}
