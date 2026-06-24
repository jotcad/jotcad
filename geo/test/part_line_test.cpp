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
            bear_path = "../../scratch/bear.stl";
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

    // 2. Perform PartLine Analysis relative to Z-up [0, 0, 1]
    std::cout << "  - Executing jot/partLine on bear.stl along [0, 0, 1]..." << std::endl;
    fs::Selector part_line_sel("jot/partLine");
    part_line_sel.parameters["$in"] = bear_shape.to_json();
    part_line_sel.parameters["dx"] = 0.0;
    part_line_sel.parameters["dy"] = 0.0;
    part_line_sel.parameters["dz"] = 1.0;
    part_line_sel = part_line_sel.with_output("$out");

    Shape result = vfs.read<Shape>(part_line_sel);

    // 3. Verify Result
    if (!result.geometry.has_value()) {
        std::cerr << "  ❌ FAIL: Output shape does not contain geometry." << std::endl;
        return 1;
    }

    Geometry line_geo = vfs.read<Geometry>(result.geometry.value());
    size_t segment_count = line_geo.segments.size();

    std::cout << "    - Parting line has " << segment_count << " segments." << std::endl;
    std::cout << "    - Parting line has " << line_geo.vertices.size() << " vertices." << std::endl;

    if (segment_count == 0) {
        std::cerr << "  ❌ FAIL: Parting line has 0 segments." << std::endl;
        return 1;
    }

    // Verify it contains no triangles/faces
    if (!line_geo.triangles.empty() || !line_geo.faces.empty()) {
        std::cerr << "  ❌ FAIL: Parting line geometry contains 2D/3D faces/triangles." << std::endl;
        return 1;
    }

    // 4. Verify optimization parameter "optimize"
    std::cout << "  - Executing jot/partLine on bear.stl with optimize=true..." << std::endl;
    fs::Selector opt_sel("jot/partLine");
    opt_sel.parameters["$in"] = bear_shape.to_json();
    opt_sel.parameters["optimize"] = true;
    opt_sel = opt_sel.with_output("$out");

    Shape opt_result = vfs.read<Shape>(opt_sel);

    if (!opt_result.geometry.has_value()) {
        std::cerr << "  ❌ FAIL: Optimized parting line shape does not contain geometry." << std::endl;
        return 1;
    }

    Geometry opt_line_geo = vfs.read<Geometry>(opt_result.geometry.value());
    size_t opt_segment_count = opt_line_geo.segments.size();
    std::cout << "    - Optimized parting line has " << opt_segment_count << " segments." << std::endl;
    std::cout << "    - Optimized parting line has " << opt_line_geo.vertices.size() << " vertices." << std::endl;

    if (opt_segment_count == 0) {
        std::cerr << "  ❌ FAIL: Optimized parting line has 0 segments." << std::endl;
        return 1;
    }

    if (!opt_result.tags.contains("dx") || !opt_result.tags.contains("dy") || !opt_result.tags.contains("dz")) {
        std::cerr << "  ❌ FAIL: Optimized output shape tags do not contain direction metadata." << std::endl;
        return 1;
    }

    double opt_dx = opt_result.tags["dx"].get<double>();
    double opt_dy = opt_result.tags["dy"].get<double>();
    double opt_dz = opt_result.tags["dz"].get<double>();
    std::cout << "    - Auto-discovered optimal direction: [" << opt_dx << ", " << opt_dy << ", " << opt_dz << "]" << std::endl;

    std::cout << "  ✅ PartLine Operator Test Passed." << std::endl;
    return 0;
}
