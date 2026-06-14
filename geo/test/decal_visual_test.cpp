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

    std::map<std::pair<int, int>, int> v_map;
    auto get_v = [&](int x, int y) {
        if (v_map.count({x, y})) return v_map[{x, y}];
        double vx = offset + x * cell_size;
        double vy = offset + y * cell_size;
        
        // Height depends on the cells this vertex is part of.
        // For simplicity, we just use the (x, y) coordinates.
        // But a vertex is shared by 4 cells. Which height?
        // Let's use the average height of neighboring cells or just x+y parity.
        double vz = ((x + y) % 2 == 0) ? 0.5 : 0.0;
        
        int idx = geo.vertices.size();
        geo.vertices.push_back({FT(vx), FT(vy), FT(vz)});
        return v_map[{x, y}] = idx;
    };

    for (int y = 0; y < cells; ++y) {
        for (int x = 0; x < cells; ++x) {
            int v0 = get_v(x, y);
            int v1 = get_v(x + 1, y);
            int v2 = get_v(x + 1, y + 1);
            int v3 = get_v(x, y + 1);
            
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
    Geometry relief_geo = create_chessboard_geometry(5.0, 10);
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
