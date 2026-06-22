#include "test_base.h"
#include "protocols.h"
#include "processor.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("grow_flat");
    register_all_ops(&vfs);

    std::cout << "Testing Grow..." << std::endl;

    // 1. Box + Box (Solid + Solid) - Verified Working
    fs::Selector box_sel;
    box_sel.path = "jot/Box";
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    box_sel = box_sel.with_output("$out");
    Shape box = vfs.read<Shape>(box_sel);

    fs::Selector tool_box_sel;
    tool_box_sel.path = "jot/Box";
    tool_box_sel.parameters["width"] = 2.0;
    tool_box_sel.parameters["height"] = 2.0;
    tool_box_sel.parameters["depth"] = 2.0;
    tool_box_sel = tool_box_sel.with_output("$out");
    Shape tool_box = vfs.read<Shape>(tool_box_sel);

    fs::Selector grow_box_sel;
    grow_box_sel.path = "jot/grow";
    grow_box_sel.parameters["$in"] = box.to_json();
    grow_box_sel.parameters["tool"] = tool_box.to_json();
    grow_box_sel = grow_box_sel.with_output("$out");

    std::cout << "  - Executing jot/grow (Box + Box)..." << std::endl;
    try {
        Shape result = vfs.read<Shape>(grow_box_sel);
        if (result.geometry.has_value()) {
            Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
            try {
                vfs.verify_well_formed_solid(res_geo, "Grow(Box, Box)");
                std::cout << "    - SUCCESS: Grow(Box, Box) is a well-formed solid." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "    - FAIL: " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "    - CAUGHT EXCEPTION (Box+Box): " << e.what() << std::endl;
    }

    // 2. Flat Rect + Flat Box (2D + 2D)
    fs::Selector rect_sel;
    rect_sel.path = "jot/Box";
    rect_sel.parameters["width"] = 10.0;
    rect_sel.parameters["height"] = 10.0;
    rect_sel.parameters["depth"] = 0.0;
    rect_sel = rect_sel.with_output("$out");
    Shape rect = vfs.read<Shape>(rect_sel);

    fs::Selector tool_flat_box_sel;
    tool_flat_box_sel.path = "jot/Box";
    tool_flat_box_sel.parameters["width"] = 2.0;
    tool_flat_box_sel.parameters["height"] = 2.0;
    tool_flat_box_sel.parameters["depth"] = 0.0;
    tool_flat_box_sel = tool_flat_box_sel.with_output("$out");
    Shape tool_flat_box = vfs.read<Shape>(tool_flat_box_sel);

    fs::Selector grow_flat_sel;
    grow_flat_sel.path = "jot/grow";
    grow_flat_sel.parameters["$in"] = rect.to_json();
    grow_flat_sel.parameters["tool"] = tool_flat_box.to_json();
    grow_flat_sel = grow_flat_sel.with_output("$out");

    std::cout << "  - Executing jot/grow (Flat Rect + Flat Box)..." << std::endl;
    try {
        Shape result = vfs.read<Shape>(grow_flat_sel);
        if (result.geometry.has_value()) {
            Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
            if (!res_geo.faces.empty()) {
                std::cout << "    - SUCCESS: Result has " << res_geo.vertices.size() << " vertices and " << res_geo.faces.size() << " faces." << std::endl;
            } else {
                std::cerr << "    - FAIL (Flat+Flat): Result has 0 faces." << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "    - FAIL (Flat+Flat): " << e.what() << std::endl;
    }

    // 3. Flat Rect + Solid Box (2D + 3D) - Verified Working
    fs::Selector grow_mixed_sel;
    grow_mixed_sel.path = "jot/grow";
    grow_mixed_sel.parameters["$in"] = rect.to_json();
    grow_mixed_sel.parameters["tool"] = tool_box.to_json();
    grow_mixed_sel = grow_mixed_sel.with_output("$out");
    
    std::cout << "  - Executing jot/grow (Flat Rect + Solid Box)..." << std::endl;
    try {
        Shape result = vfs.read<Shape>(grow_mixed_sel);
        if (result.geometry.has_value()) {
            Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
            vfs.verify_well_formed_solid(res_geo, "Grow(Flat Rect, Solid Box)");
            std::cout << "    - SUCCESS: Grow(Flat Rect, Solid Box) is a well-formed solid." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "    - FAIL (Flat+Solid): " << e.what() << std::endl;
    }

    // 4. Solid Box + Flat Square (3D + 2D) - NEW
    fs::Selector grow_cube_square_sel;
    grow_cube_square_sel.path = "jot/grow";
    grow_cube_square_sel.parameters["$in"] = box.to_json();
    grow_cube_square_sel.parameters["tool"] = tool_flat_box.to_json();
    grow_cube_square_sel = grow_cube_square_sel.with_output("$out");

    std::cout << "  - Executing jot/grow (Solid Box + Flat Square)..." << std::endl;
    try {
        Shape result = vfs.read<Shape>(grow_cube_square_sel);
        if (result.geometry.has_value()) {
            Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
            try {
                vfs.verify_well_formed_solid(res_geo, "Grow(Solid Box, Flat Square)");
                std::cout << "    - SUCCESS: Grow(Solid Box, Flat Square) is a well-formed solid." << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "    - FAIL (Solid+Flat): " << e.what() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "    - FAIL (Solid+Flat): " << e.what() << std::endl;
    }

    // 5. Concave Shape + Box Tool -> Concave Minkowski Sum
    // Subject is a U-bracket: Box(20, 20, 20) cut by Box(12, 30, 12)
    // Tool is a Box(2, 2, 2)
    // Result should be concave, and the point (0, 0, 0) should remain outside the grown shape.
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

    fs::Selector grow_concave_sel;
    grow_concave_sel.path = "jot/grow";
    grow_concave_sel.parameters["$in"] = concave_bracket.to_json();
    grow_concave_sel.parameters["tool"] = tool_box.to_json(); // Box(2, 2, 2)
    grow_concave_sel = grow_concave_sel.with_output("$out");

    std::cout << "  - Executing jot/grow (Concave Bracket + Box)..." << std::endl;
    try {
        Shape result = vfs.read<Shape>(grow_concave_sel);
        if (result.geometry.has_value()) {
            Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
            vfs.verify_well_formed_solid(res_geo, "Grow(Concave Bracket, Box)");

            // Verify that the cavity at (0,0,0) is preserved (point is outside the mesh)
            boolean::Surface_mesh res_mesh = boolean::Engine::geometry_to_mesh(res_geo);
            CGAL::Side_of_triangle_mesh<boolean::Surface_mesh, EK> inside_check(res_mesh);
            auto side = inside_check(EK::Point_3(0, 0, 0));
            if (side == CGAL::ON_UNBOUNDED_SIDE) {
                std::cout << "    - SUCCESS: Point (0,0,0) is outside the grown shape (Cavity Preserved)." << std::endl;
            } else {
                std::cerr << "    - FAIL: Point (0,0,0) is inside the grown shape (Cavity Filled)." << std::endl;
                exit(1);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "    - FAIL (Concave): " << e.what() << std::endl;
        exit(1);
    }

    return 0;
}
