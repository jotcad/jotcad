#include "test_base.h"
#include "render/rasterizer.h"
#include <fstream>
#include <cmath>

using namespace jotcad::geo;

void test_convex_visuals() {
    MockVFS vfs("convex_visuals");
    register_all_ops(&vfs);

    std::cout << "  - Creating Cube Subject..." << std::endl;
    fs::Selector box_20_sel("jot/Box");
    box_20_sel.parameters = {{"width", 20.0}, {"height", 20.0}, {"depth", 20.0}};
    box_20_sel.output = "$out";
    Processor::execute(&vfs, box_20_sel);
    Shape box_20 = vfs.read<Shape>(box_20_sel);

    fs::Selector box_cut_sel("jot/Box");
    box_cut_sel.parameters = {{"width", 12.0}, {"height", 30.0}, {"depth", 12.0}};
    box_cut_sel.output = "$out";
    Processor::execute(&vfs, box_cut_sel);
    Shape box_cut = vfs.read<Shape>(box_cut_sel);

    fs::Selector cut_sel("jot/cut");
    cut_sel.parameters = {
        {"$in", box_20.to_json()},
        {"tools", nlohmann::json::array({box_cut.to_json()})}
    };
    cut_sel.output = "$out";
    Processor::execute(&vfs, cut_sel);
    Shape concave_bracket = vfs.read<Shape>(cut_sel);

    std::cout << "  - Executing jot/convex..." << std::endl;
    fs::Selector convex_sel("jot/convex");
    convex_sel.parameters = {{"$in", concave_bracket.to_json()}};
    convex_sel.output = "$out";
    
    try {
        Processor::execute(&vfs, convex_sel);
        Shape result = vfs.read<Shape>(convex_sel);

        // Explode and color the 5 parts
        std::vector<Shape> colored_parts;
        std::vector<std::string> colors = {"red", "green", "blue", "yellow", "magenta"};
        std::vector<std::vector<double>> shifts = {
            {6.0, 0.0, 6.0},
            {-6.0, 0.0, 6.0},
            {6.0, 0.0, -6.0},
            {-6.0, 0.0, -6.0},
            {0.0, 0.0, 0.0}
        };

        size_t part_idx = 0;
        for (const auto& comp : result.components) {
            // Skip non-geometry children like ghost tools
            if (!comp.geometry.has_value()) continue;
            Shape part = comp;
            part.tf = Matrix::translate(shifts[part_idx][0], shifts[part_idx][1], shifts[part_idx][2]) * part.tf;
            part.add_tag("color", colors[part_idx]);
            colored_parts.push_back(part);
            part_idx++;
            if (part_idx >= colors.size()) break;
        }

        Shape exploded_group;
        exploded_group.components = colored_parts;
        exploded_group.add_tag("type", "group");

        std::cout << "  - Rendering Result..." << std::endl;
        // Use an angled isometric-like view for the final PNG
        exploded_group.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);
        
        auto png_data = Rasterizer::render_png(&vfs, exploded_group, 512, 512, 0.0, 0.0);
        if (!png_data.empty()) {
            std::filesystem::create_directories("actual");
            std::ofstream out("actual/convex_exploded_bracket.png", std::ios::binary);
            out.write((const char*)png_data.data(), png_data.size());
            std::cout << "  📸 Saved actual/convex_exploded_bracket.png" << std::endl;
        } else {
            std::cerr << "  ⚠️  Failed to render PNG (Empty Geometry)" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "  ❌ Convex visual test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    test_convex_visuals();
    return 0;
}
