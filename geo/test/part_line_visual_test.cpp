#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include <fstream>
#include <cmath>

using namespace jotcad::geo;

void test_part_line_visuals() {
    MockVFS vfs("part_line_visuals");
    register_all_ops(&vfs);

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
        return;
    }
    std::cout << "    - Loaded successfully with " << bear_geo.triangles.size() << " triangles." << std::endl;

    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, json::object());

    // 2. Perform PartLine Analysis with optimize=true
    std::cout << "  - Executing jot/partLine with optimize=true..." << std::endl;
    fs::Selector part_line_sel("jot/partLine");
    part_line_sel.parameters["$in"] = bear_shape.to_json();
    part_line_sel.parameters["optimize"] = true;
    part_line_sel.output = "$out";

    try {
        Processor::execute(&vfs, part_line_sel);
        Shape part_line_result = vfs.read<Shape>(part_line_sel);

        if (part_line_result.tags.contains("dx")) {
            std::cout << "    - Discovered direction vector: ["
                      << part_line_result.tags["dx"].get<double>() << ", "
                      << part_line_result.tags["dy"].get<double>() << ", "
                      << part_line_result.tags["dz"].get<double>() << "]" << std::endl;
        }

        // 3. Color the bear light gray so we can see the green parting line on top
        std::cout << "  - Coloring the bear mesh light gray..." << std::endl;
        fs::Selector color_sel("jot/color");
        color_sel.parameters["$in"] = bear_shape.to_json();
        color_sel.parameters["color"] = "#cccccc";
        color_sel.output = "$out";
        Processor::execute(&vfs, color_sel);
        Shape gray_bear = vfs.read<Shape>(color_sel);

        // 4. Create a composite shape containing both the gray bear and the parting line
        Shape composite;
        composite.components.push_back(gray_bear);
        composite.components.push_back(part_line_result);

        std::cout << "  - Rendering composite shape..." << std::endl;
        // Use an angled isometric-like view for the final PNG
        composite.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

        // Render at 1024x1024
        auto png_data = Rasterizer::render_png(&vfs, composite, 1024, 1024, 0.0, 0.0);
        if (!png_data.empty()) {
            std::filesystem::create_directories("actual");
            std::ofstream out("actual/bear_part_line_optimal.png", std::ios::binary);
            out.write((const char*)png_data.data(), png_data.size());
            std::cout << "  📸 Saved actual/bear_part_line_optimal.png" << std::endl;
        } else {
            std::cerr << "  ⚠️  Failed to render PNG (Empty Geometry)" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "  ❌ PartLine visual test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    test_part_line_visuals();
    return 0;
}
