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
    mold_sel.parameters["draft"] = 0.0;
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape box_mold = vfs.read<Shape>(mold_sel);

    assert(box_mold.components.size() >= 2); // mold piece 1 + 1 original shape
    assert(box_mold.components[0].tags.contains("mold_piece") && box_mold.components[0].tags["mold_piece"] == 1);
    std::cout << "    - Produced 1st demoldable pillar for Box successfully." << std::endl;

    // 2. Test first demoldable pillar on bear.stl with explode
    std::cout << "  - Testing 1st demoldable pillar on bear.stl with explode=15.0..." << std::endl;
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
    bear_mold_sel.parameters["draft"] = 1.0 / 360.0;
    bear_mold_sel.output = "$out";

    Processor::execute(&vfs, bear_mold_sel);
    Shape bear_mold = vfs.read<Shape>(bear_mold_sel);

    assert(bear_mold.components.size() >= 2); // mold piece 1 + 1 original shape
    assert(bear_mold.components[0].tags.contains("mold_piece") && bear_mold.components[0].tags["mold_piece"] == 1);
    std::cout << "    - Produced 1st demoldable pillar for Bear successfully." << std::endl;
    std::cout << "  ✅ jot/mold test passed." << std::endl;
    return 0;
}
