#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "infra/stl.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("undercut");
    register_all_ops(&vfs);

    std::cout << "Testing Undercut Operator..." << std::endl;

    // 1. Create a 10x10x10 Box
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    box_sel = box_sel.with_output("$out");
    Shape box = vfs.read<Shape>(box_sel);

    // 2. Perform Undercut Analysis relative to Z-up [0, 0, 1]
    std::cout << "  - Executing jot/undercut on Box along [0, 0, 1]..." << std::endl;
    fs::Selector undercut_sel("jot/undercut");
    undercut_sel.parameters["$in"] = box.to_json();
    undercut_sel.parameters["dx"] = 0.0;
    undercut_sel.parameters["dy"] = 0.0;
    undercut_sel.parameters["dz"] = 1.0;
    undercut_sel = undercut_sel.with_output("$out");

    Shape result = vfs.read<Shape>(undercut_sel);

    // Verify Box Result
    // A box has 12 triangles (2 per face):
    // Top face (Z+) -> 2 triangles -> safe_faces (green)
    // Bottom face (Z-) -> 2 triangles -> undercut_faces (red)
    // Side faces (X+, X-, Y+, Y-) -> 8 triangles -> flat_faces (yellow)
    
    if (result.components.size() != 3) {
        std::cerr << "  ❌ FAIL: Expected 3 component sub-meshes (safe, undercut, flat) in Box group, got: " << result.components.size() << std::endl;
        return 1;
    }

    for (const auto& comp : result.components) {
        std::string name = comp.tags.value("name", "");
        std::string color = comp.tags.value("color", "");
        Geometry geo = vfs.read<Geometry>(comp.geometry.value());
        size_t tri_count = geo.triangles.size();

        if (name == "safe_faces") {
            if (color != "#2bee2b") {
                std::cerr << "  ❌ FAIL: safe_faces has wrong color: " << color << std::endl;
                return 1;
            }
            if (tri_count != 2) {
                std::cerr << "  ❌ FAIL: safe_faces has wrong triangle count: " << tri_count << " (expected 2)" << std::endl;
                return 1;
            }
            std::cout << "    - safe_faces (top) verified with 2 triangles." << std::endl;
        } else if (name == "undercut_faces") {
            if (color != "#ee2b2b") {
                std::cerr << "  ❌ FAIL: undercut_faces has wrong color: " << color << std::endl;
                return 1;
            }
            if (tri_count != 2) {
                std::cerr << "  ❌ FAIL: undercut_faces has wrong triangle count: " << tri_count << " (expected 2)" << std::endl;
                return 1;
            }
            std::cout << "    - undercut_faces (bottom) verified with 2 triangles." << std::endl;
        } else if (name == "flat_faces") {
            if (color != "#eeee2b") {
                std::cerr << "  ❌ FAIL: flat_faces has wrong color: " << color << std::endl;
                return 1;
            }
            if (tri_count != 8) {
                std::cerr << "  ❌ FAIL: flat_faces has wrong triangle count: " << tri_count << " (expected 8)" << std::endl;
                return 1;
            }
            std::cout << "    - flat_faces (sides) verified with 8 triangles." << std::endl;
        }
    }

    // 3. Test on bunny.stl
    std::string bunny_path = "scratch/bunny.stl";
    {
        std::ifstream check(bunny_path);
        if (!check.good()) {
            bunny_path = "../../scratch/bunny.stl";
        }
    }
    std::cout << "  - Loading " << bunny_path << "..." << std::endl;
    Geometry bunny_geo;
    if (!STLReader::read_file(bunny_path, bunny_geo)) {
        std::cerr << "  ❌ FAIL: Could not load bunny.stl from " << bunny_path << std::endl;
        return 1;
    }
    std::cout << "    - Loaded bunny.stl successfully with " << bunny_geo.triangles.size() << " triangles." << std::endl;

    Shape bunny_shape = JotVfsProtocol::make_shape(&vfs, bunny_geo, json::object());

    std::cout << "  - Executing jot/undercut on bunny.stl along [0, 0, 1]..." << std::endl;
    undercut_sel.parameters["$in"] = bunny_shape.to_json();
    Shape bunny_result = vfs.read<Shape>(undercut_sel);

    if (bunny_result.components.empty()) {
        std::cerr << "  ❌ FAIL: Output shape components are empty for bunny.stl" << std::endl;
        return 1;
    }

    size_t total_out_triangles = 0;
    for (const auto& comp : bunny_result.components) {
        Geometry geo = vfs.read<Geometry>(comp.geometry.value());
        total_out_triangles += geo.triangles.size();
        std::cout << "    - Component '" << comp.tags.value("name", "") << "' has " << geo.triangles.size() << " triangles (" << comp.tags.value("color", "") << ")." << std::endl;
    }

    if (total_out_triangles != bunny_geo.triangles.size()) {
        std::cerr << "  ❌ FAIL: Triangle count mismatch. Expected " << bunny_geo.triangles.size() << ", got " << total_out_triangles << std::endl;
        return 1;
    }

    std::cout << "  ✅ Undercut Operator Test Passed." << std::endl;
    return 0;
}
