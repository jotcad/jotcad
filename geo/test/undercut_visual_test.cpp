#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include <fstream>
#include <cmath>

using namespace jotcad::geo;

void test_undercut_visuals() {
    MockVFS vfs("undercut_visuals");
    register_all_ops(&vfs);

    // 1. Load the bunny STL
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
        return;
    }
    std::cout << "    - Loaded successfully with " << bunny_geo.triangles.size() << " triangles." << std::endl;

    Shape bunny_shape = JotVfsProtocol::make_shape(&vfs, bunny_geo, json::object());

    // 2. Perform Undercut Analysis relative to Z-up [0, 0, 1]
    std::cout << "  - Executing jot/undercut..." << std::endl;
    fs::Selector undercut_sel("jot/undercut");
    undercut_sel.parameters["$in"] = bunny_shape.to_json();
    undercut_sel.parameters["dx"] = 0.0;
    undercut_sel.parameters["dy"] = 0.0;
    undercut_sel.parameters["dz"] = 1.0;
    undercut_sel.output = "$out";

    try {
        Processor::execute(&vfs, undercut_sel);
        Shape undercut_result = vfs.read<Shape>(undercut_sel);

        // 3. Execute jot/rainbow on the resulting mold parts
        std::cout << "  - Executing jot/rainbow on the mold parts..." << std::endl;
        fs::Selector rainbow_sel("jot/rainbow");
        rainbow_sel.parameters = {{"$in", undercut_result.to_json()}};
        rainbow_sel.output = "$out";

        Processor::execute(&vfs, rainbow_sel);
        Shape final_result = vfs.read<Shape>(rainbow_sel);

        std::cout << "  - Rendering Result..." << std::endl;
        // Use an angled isometric-like view for the final PNG
        final_result.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

        // Render at 1024x1024
        auto png_data = Rasterizer::render_png(&vfs, final_result, 1024, 1024, 0.0, 0.0);
        if (!png_data.empty()) {
            std::filesystem::create_directories("actual");
            std::ofstream out("actual/bunny_undercuts.png", std::ios::binary);
            out.write((const char*)png_data.data(), png_data.size());
            std::cout << "  📸 Saved actual/bunny_undercuts.png" << std::endl;
        } else {
            std::cerr << "  ⚠️  Failed to render PNG (Empty Geometry)" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "  ❌ Undercut visual test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    test_undercut_visuals();
    return 0;
}
