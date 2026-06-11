#include "test_base.h"
#include "render/rasterizer.h"
#include <fstream>
#include <cmath>

using namespace jotcad::geo;

// Helper to create a 3D chessboard relief mesh directly
Geometry create_chessboard_geometry(double size, int cells) {
    Geometry geo;
    double cell_size = size / cells;
    double offset = -size / 2.0;

    for (int y = 0; y < cells; ++y) {
        for (int x = 0; x < cells; ++x) {
            double x0 = offset + x * cell_size;
            double y0 = offset + y * cell_size;
            double x1 = x0 + cell_size;
            double y1 = y0 + cell_size;
            
            // Alternating height: 0.0 or 0.2
            double z = ((x + y) % 2 == 0) ? 0.5 : 0.0;

            int v0 = geo.vertices.size();
            geo.vertices.push_back({FT(x0), FT(y0), FT(z)});
            int v1 = geo.vertices.size();
            geo.vertices.push_back({FT(x1), FT(y0), FT(z)});
            int v2 = geo.vertices.size();
            geo.vertices.push_back({FT(x1), FT(y1), FT(z)});
            int v3 = geo.vertices.size();
            geo.vertices.push_back({FT(x0), FT(y1), FT(z)});
            
            geo.triangles.push_back({v0, v1, v2});
            geo.triangles.push_back({v0, v2, v3});
        }
    }
    return geo;
}

void test_decal_visuals() {
    MockVFS vfs("decal_visuals");
    register_all_ops(&vfs);

    std::cout << "  - Creating Cube Subject..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters = {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}};
    box_sel.output = "$out";
    Processor::execute(&vfs, box_sel);
    Shape subject = vfs.read<Shape>(box_sel);
    // Apply a slight tilt to see wrapping
    subject.tf = Matrix::rotationX(0.01) * Matrix::rotationY(0.01);

    std::cout << "  - Creating Chessboard Relief..." << std::endl;
    Geometry relief_geo = create_chessboard_geometry(100.0, 20);
    Shape relief;
    relief.geometry = vfs.materialize<Geometry>(relief_geo);
    // Position relief at Z=10 world, looking down
    relief.tf = Matrix::translate(0, 0, 10.0);

    std::cout << "  - Executing jot/decal..." << std::endl;
    fs::Selector decal_sel("jot/decal");
    decal_sel.parameters = {
        {"$in", subject.to_json()},
        {"relief", relief.to_json()},
        {"seam", "skirting"}
    };
    decal_sel.output = "$out";
    
    try {
        Processor::execute(&vfs, decal_sel);
        Shape result = vfs.read<Shape>(decal_sel);

        std::cout << "  - Rendering Result..." << std::endl;
        // Use an angled view for the final PNG
        result.tf = Matrix::rotationX(-0.1) * Matrix::rotationY(0.125);
        
        auto png_data = Rasterizer::render_png(&vfs, result, 512, 512, 0.0, 0.0);
        if (!png_data.empty()) {
            std::filesystem::create_directories("actual");
            std::ofstream out("actual/decal_chessboard_wrap.png", std::ios::binary);
            out.write((const char*)png_data.data(), png_data.size());
            std::cout << "  📸 Saved actual/decal_chessboard_wrap.png" << std::endl;
        } else {
            std::cerr << "  ⚠️  Failed to render PNG (Empty Geometry)" << std::endl;
        }

    } catch (const std::exception& e) {
        std::cerr << "  ❌ Decal visual test failed: " << e.what() << std::endl;
        throw;
    }
}

int main() {
    test_decal_visuals();
    return 0;
}
