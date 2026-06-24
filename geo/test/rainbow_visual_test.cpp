#include "test_base.h"
#include "render/rasterizer.h"
#include <fstream>
#include <cmath>
#include <vector>

using namespace jotcad::geo;

void test_rainbow_visuals() {
    MockVFS vfs("rainbow_visuals");
    register_all_ops(&vfs);

    std::cout << "  - Creating 12 boxes in a row..." << std::endl;
    std::vector<Shape> components;
    for (int i = 0; i < 12; ++i) {
        fs::Selector box_sel("jot/Box");
        box_sel.parameters = {{"width", 5.0}, {"height", 20.0}, {"depth", 5.0}};
        box_sel.output = "$out";
        Processor::execute(&vfs, box_sel);
        Shape box = vfs.read<Shape>(box_sel);
        
        // Offset each box along the X axis so they align nicely side-by-side
        box.tf = Matrix::translate((i - 5.5) * 8.0, 0.0, 0.0) * box.tf;
        components.push_back(box);
    }

    Shape group;
    group.components = components;
    group.add_tag("type", "group");

    std::cout << "  - Executing jot/rainbow..." << std::endl;
    fs::Selector rainbow_sel("jot/rainbow");
    rainbow_sel.parameters = {{"$in", group.to_json()}};
    rainbow_sel.output = "$out";
    
    try {
        Processor::execute(&vfs, rainbow_sel);
        Shape result = vfs.read<Shape>(rainbow_sel);

        std::cout << "  - Rendering Result..." << std::endl;
        // Use an isometric view
        result.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);
        
        auto png_data = Rasterizer::render_png(&vfs, result, 1024, 512, 0.0, 0.0);
        if (!png_data.empty()) {
            std::filesystem::create_directories("actual");
            std::ofstream out("actual/rainbow_colors.png", std::ios::binary);
            out.write((const char*)png_data.data(), png_data.size());
            std::cout << "  📸 Saved actual/rainbow_colors.png" << std::endl;
        } else {
            std::cerr << "  ⚠️  Failed to render PNG (Empty Geometry)" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "  ❌ Rainbow visual test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    test_rainbow_visuals();
    return 0;
}
