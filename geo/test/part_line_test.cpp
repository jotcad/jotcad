#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "infra/stl.h"
#include <fstream>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("part_line");
    register_all_ops(&vfs);

    std::cout << "Testing PartLine Operator on Bear..." << std::endl;

    // 1. Load the bear STL
    std::string bear_path = "scratch/bear.stl";
    {
        std::ifstream check(bear_path);
        if (!check.good()) {
            check.open("../scratch/bear.stl");
            if (check.good()) {
                bear_path = "../scratch/bear.stl";
            } else {
                bear_path = "../../scratch/bear.stl";
            }
        }
    }
    std::cout << "  - Loading " << bear_path << "..." << std::endl;
    Geometry bear_geo;
    if (!STLReader::read_file(bear_path, bear_geo)) {
        std::cerr << "  ❌ FAIL: Could not load bear.stl from " << bear_path << std::endl;
        return 1;
    }
    std::cout << "    - Loaded bear.stl successfully with " << bear_geo.triangles.size() << " triangles." << std::endl;

    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, json::object());

    // 2. Perform PartLine Analysis on bear.stl along [0, 0, 1] (Expect Demoldability Error)
    std::cout << "  - Verifying jot/partLine on bear.stl along [0, 0, 1] throws Demoldability Error..." << std::endl;
    fs::Selector part_line_sel("jot/partLine");
    part_line_sel.parameters["$in"] = bear_shape.to_json();
    part_line_sel.parameters["dx"] = 0.0;
    part_line_sel.parameters["dy"] = 0.0;
    part_line_sel.parameters["dz"] = 1.0;
    part_line_sel = part_line_sel.with_output("$out");

    bool threw_z_error = false;
    try {
        Processor::execute(&vfs, part_line_sel);
    } catch (const std::runtime_error& e) {
        threw_z_error = true;
        std::cout << "    - Caught expected demoldability error along [0, 0, 1]: " << e.what() << std::endl;
    }
    if (!threw_z_error) {
        std::cerr << "  ❌ FAIL: Expected Demoldability Error on bear along [0, 0, 1]." << std::endl;
        return 1;
    }

    // 3. Verify optimization parameter "optimize=true" on bear.stl (Expect Demoldability Error)
    std::cout << "  - Verifying jot/partLine on bear.stl with optimize=true throws Demoldability Error..." << std::endl;
    fs::Selector opt_sel("jot/partLine");
    opt_sel.parameters["$in"] = bear_shape.to_json();
    opt_sel.parameters["optimize"] = true;
    opt_sel = opt_sel.with_output("$out");

    bool threw_opt_error = false;
    try {
        Processor::execute(&vfs, opt_sel);
    } catch (const std::runtime_error& e) {
        threw_opt_error = true;
        std::cout << "    - Caught expected demoldability error during optimize: " << e.what() << std::endl;
    }
    if (!threw_opt_error) {
        std::cerr << "  ❌ FAIL: Expected Demoldability Error on non-2-piece bear shape during optimize=true." << std::endl;
        return 1;
    }

    // 4. Verify partLine succeeds on 2-piece Box
    std::cout << "  - Verifying jot/partLine succeeds on Box..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    Shape box = vfs.read<Shape>(box_sel.with_output("$out"));

    fs::Selector box_opt_sel("jot/partLine");
    box_opt_sel.parameters["$in"] = box.to_json();
    box_opt_sel.parameters["optimize"] = true;
    Shape box_parting = vfs.read<Shape>(box_opt_sel.with_output("$out"));

    if (!box_parting.geometry.has_value()) {
        std::cerr << "  ❌ FAIL: Box parting line shape has no geometry." << std::endl;
        return 1;
    }

    std::cout << "  ✅ PartLine Operator Test Passed." << std::endl;
    return 0;
}
