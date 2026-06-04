#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include "../../fs/cpp/vendor/stb_image_write.h"
#include <fstream>
#include <cmath>
#include <cassert>

extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("relief");
    register_all_ops(&vfs);

    std::cout << "Testing Relief (Heightmap to Mesh) Operation..." << std::endl;

    // 1. Generate a 2x2 mock grayscale image
    // Channels: RGB (3 bytes per pixel)
    // Pixel 0 (top-left): Black (0)
    // Pixel 1 (top-right): Dark gray (85)
    // Pixel 2 (bottom-left): Light gray (170)
    // Pixel 3 (bottom-right): White (255)
    unsigned char pixels[12] = {
        0, 0, 0,
        85, 85, 85,
        170, 170, 170,
        255, 255, 255
    };

    int len = 0;
    unsigned char* png_data = stbi_write_png_to_mem(pixels, 2 * 3, 2, 2, 3, &len);
    if (!png_data) {
        std::cerr << "❌ Failed to generate test PNG" << std::endl;
        return 1;
    }
    
    std::vector<uint8_t> png_bytes(png_data, png_data + len);
    free(png_data);

    // Write PNG bytes to VFS under a known selector/CID
    Selector image_sel = Selector{"jot/Image", {{"url", "mock://test.png"}}}.with_output("$out");
    vfs.write(image_sel, png_bytes);
    std::cout << "  - Wrote mock PNG of size " << png_bytes.size() << " bytes to VFS" << std::endl;

    // 2. Execute ReliefOp
    // We request a 2x2 resolution, width=10, height=10, depth=2.0, base=1.0
    Selector relief_sel = Selector{"jot/Relief", {
        {"image", image_sel.to_json()},
        {"width", 10.0},
        {"height", 10.0},
        {"depth", 2.0},
        {"base", 1.0},
        {"resolution", 2}
    }}.with_output("$out");

    std::cout << "  - Executing ReliefOp..." << std::endl;
    try {
        Processor::execute(&vfs, relief_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ ReliefOp execution failed: " << e.what() << std::endl;
        return 1;
    }

    // 3. Read and verify shape
    try {
        Shape result = vfs.read<Shape>(relief_sel);
        if (!result.geometry.has_value()) {
            std::cerr << "❌ Relief result does not contain geometry" << std::endl;
            return 1;
        }

        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        std::cout << "  - Vertices count: " << geo.vertices.size() << std::endl;
        std::cout << "  - Triangles count: " << geo.triangles.size() << std::endl;

        assert(geo.vertices.size() == 8);

        // Expected Top Z values (base + intensity * depth):
        // Intensity values for 0, 85, 170, 255 are approximately 0.0, 0.333, 0.666, 1.0.
        // base = 1.0, depth = 2.0
        // Vertex 0: Z = 1.0 + 0.0 * 2.0 = 1.0
        // Vertex 1: Z = 1.0 + 0.333 * 2.0 = 1.666
        // Vertex 2: Z = 1.0 + 0.666 * 2.0 = 2.333
        // Vertex 3: Z = 1.0 + 1.0 * 2.0 = 3.0
        
        double z0 = CGAL::to_double(geo.vertices[0].z);
        double z1 = CGAL::to_double(geo.vertices[1].z);
        double z2 = CGAL::to_double(geo.vertices[2].z);
        double z3 = CGAL::to_double(geo.vertices[3].z);

        std::cout << "  - Vertex 0 Z (Black): " << z0 << " (Expected ~1.0)" << std::endl;
        std::cout << "  - Vertex 1 Z (Dark gray): " << z1 << " (Expected ~1.666)" << std::endl;
        std::cout << "  - Vertex 2 Z (Light gray): " << z2 << " (Expected ~2.333)" << std::endl;
        std::cout << "  - Vertex 3 Z (White): " << z3 << " (Expected ~3.0)" << std::endl;

        assert(std::abs(z0 - 1.0) < 0.01);
        assert(std::abs(z1 - 1.666) < 0.02);
        assert(std::abs(z2 - 2.333) < 0.02);
        assert(std::abs(z3 - 3.0) < 0.01);

        for (int i = 4; i < 8; ++i) {
            double z_bot = CGAL::to_double(geo.vertices[i].z);
            assert(std::abs(z_bot - 0.0) < 1e-9);
        }

        // 4. Render verification PNG
        Selector png_addr = Selector{"jot/png", {
            {"$in", relief_sel.to_json()},
            {"ax", 0.61547}, 
            {"ay", 0.78539}
        }}.with_output("$out");

        std::cout << "  - Generating verification PNG (actual/relief_test_final.png)..." << std::endl;
        Processor::execute(&vfs, png_addr);
        
        std::vector<uint8_t> png_render_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/relief_test_final.png", std::ios::binary);
        out.write((char*)png_render_bytes.data(), png_render_bytes.size());
        out.close();
        std::cout << "  - Saved verification PNG to actual/relief_test_final.png" << std::endl;

        std::cout << "✅ Relief Test PASS" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Relief verification failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
