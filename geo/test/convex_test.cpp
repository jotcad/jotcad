#include "test_base.h"
#include "protocols.h"
#include "processor.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("convex");
    register_all_ops(&vfs);

    std::cout << "Testing Convex Operator (Convex Decomposition)..." << std::endl;

    // 1. U-bracket (non-convex shape)
    fs::Selector box_20_sel;
    box_20_sel.path = "jot/Box";
    box_20_sel.parameters["width"] = 20.0;
    box_20_sel.parameters["height"] = 20.0;
    box_20_sel.parameters["depth"] = 20.0;
    box_20_sel = box_20_sel.with_output("$out");
    Shape box_20 = vfs.read<Shape>(box_20_sel);

    fs::Selector box_cut_sel;
    box_cut_sel.path = "jot/Box";
    box_cut_sel.parameters["width"] = 12.0;
    box_cut_sel.parameters["height"] = 30.0;
    box_cut_sel.parameters["depth"] = 12.0;
    box_cut_sel = box_cut_sel.with_output("$out");
    Shape box_cut = vfs.read<Shape>(box_cut_sel);

    fs::Selector cut_sel;
    cut_sel.path = "jot/cut";
    cut_sel.parameters["$in"] = box_20.to_json();
    cut_sel.parameters["tools"] = nlohmann::json::array({box_cut.to_json()});
    cut_sel = cut_sel.with_output("$out");
    Shape concave_bracket = vfs.read<Shape>(cut_sel);

    // Verify it is not convex originally
    {
        Geometry bracket_geo = vfs.read<Geometry>(concave_bracket.geometry.value());
        boolean::Surface_mesh bracket_mesh = boolean::Engine::geometry_to_mesh(bracket_geo);
        if (CGAL::is_strongly_convex_3(bracket_mesh)) {
            std::cerr << "  ❌ FAIL: Original U-bracket is incorrectly detected as convex." << std::endl;
            return 1;
        } else {
            std::cout << "  - Verified original bracket is concave." << std::endl;
        }
    }

    // 2. Perform Convex Decomposition
    fs::Selector convex_sel;
    convex_sel.path = "jot/convex";
    convex_sel.parameters["$in"] = concave_bracket.to_json();
    convex_sel = convex_sel.with_output("$out");

    std::cout << "  - Executing jot/convex..." << std::endl;
    Shape result = vfs.read<Shape>(convex_sel);

    // 3. Verify Result
    if (result.components.size() <= 1) {
        std::cerr << "  ❌ FAIL: Expected multiple convex components, got: " << result.components.size() << std::endl;
        return 1;
    }

    std::cout << "  - Result has " << result.components.size() << " components." << std::endl;
    for (size_t i = 0; i < result.components.size(); ++i) {
        const auto& comp = result.components[i];
        if (!comp.geometry.has_value()) {
            std::cerr << "  ❌ FAIL: Component " << i << " has no geometry." << std::endl;
            return 1;
        }
        Geometry comp_geo = vfs.read<Geometry>(comp.geometry.value());
        boolean::Surface_mesh comp_mesh = boolean::Engine::geometry_to_mesh(comp_geo);
        
        // Assert that the component is a solid
        vfs.verify_well_formed_solid(comp_geo, "Convex Component " + std::to_string(i));
        
        // Assert that the component is convex
        if (!CGAL::is_strongly_convex_3(comp_mesh)) {
            std::cerr << "  ❌ FAIL: Component " << i << " is not strongly convex." << std::endl;
            return 1;
        } else {
            std::cout << "    - Component " << i << " is strongly convex." << std::endl;
        }
    }

    std::cout << "  ✅ Convex Operator Test Passed." << std::endl;
    return 0;
}
