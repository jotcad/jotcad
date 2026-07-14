#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include "../../fs/cpp/vendor/stb_image_write.h"
#include <fstream>
#include <cmath>
#include <cassert>

extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

using namespace jotcad::geo;
using namespace fs;

std::vector<uint8_t> read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        file.open("test/Heightmap_of_Trencrom_Hill.png", std::ios::binary);
    }
    if (!file) {
        file.open("Heightmap_of_Trencrom_Hill.png", std::ios::binary);
    }
    if (!file) {
        file.open("geo/test/Heightmap_of_Trencrom_Hill.png", std::ios::binary);
    }
    if (!file) {
        throw std::runtime_error("Test helper: Failed to read local file: " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
}

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
    // We request a width=10, height=10, depth=2.0, base=1.0
    Selector relief_sel = Selector{"jot/relief", {
        {"$in", image_sel.to_json()},
        {"width", 10.0},
        {"breadth", 10.0},
        {"height", 2.0},
        {"base", 1.0},
        {"minArea", 0.0}
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

        assert(geo.vertices.size() > 0);
        assert(geo.triangles.size() > 0);

        bool found_0 = false, found_1 = false, found_1_666 = false, found_2_333 = false, found_3 = false;
        for (const auto& v : geo.vertices) {
            double z = CGAL::to_double(v.z);
            if (std::abs(z - 0.0) < 1e-5) found_0 = true;
            else if (std::abs(z - 1.0) < 0.02) found_1 = true;
            else if (std::abs(z - 1.666) < 0.02) found_1_666 = true;
            else if (std::abs(z - 2.333) < 0.02) found_2_333 = true;
            else if (std::abs(z - 3.0) < 0.02) found_3 = true;
        }
        assert(found_0);
        assert(found_1);
        assert(found_1_666);
        assert(found_2_333);
        assert(found_3);

        vfs.verify_well_formed_solid(geo, "Quantized Relief Case 1 Grid");

        // 4. Render verification PNG
        Selector png_addr = Selector{"jot/png", {
            {"$in", relief_sel.to_json()},
            {"ax", -0.61547}, 
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

        // 5. Test Case 2: Fetching a non-image page instead of an image
        std::cout << "  - Executing Test Case 2 (Non-image fetch failure)..." << std::endl;
        try {
            std::string html_url = "mock://invalid-image";
            Selector image2_sel = Selector{"jot/Image", {{"url", html_url}}}.with_output("$out");
            
            std::cout << "    - Executing ImageOp on URL: " << html_url << std::endl;
            Processor::execute(&vfs, image2_sel);
            
            std::cerr << "❌ FAIL: Expected ImageOp to throw error on non-image content, but it succeeded!" << std::endl;
            return 1;
        } catch (const std::exception& e) {
            std::cout << "    - SUCCESS: Caught expected error: " << e.what() << std::endl;
        }

        // 6. Test Case 3: Fetching the correct PNG heightmap and building a relief
        std::cout << "  - Executing Test Case 3 (Real heightmap fetch and relief generation)..." << std::endl;
        try {
            std::string valid_url = "mock://Heightmap_of_Trencrom_Hill.png";
            Selector image3_sel = Selector{"jot/Image", {{"url", valid_url}}}.with_output("$out");
            
            std::cout << "    - Reading local heightmap image and pre-populating VFS..." << std::endl;
            std::vector<uint8_t> real_img_bytes = read_file_bytes("geo/test/Heightmap_of_Trencrom_Hill.png");
            vfs.write(image3_sel, real_img_bytes);

            std::cout << "    - Executing ImageOp on valid URL: " << valid_url << std::endl;
            Processor::execute(&vfs, image3_sel);
            
            // Generate Relief from the fetched heightmap
            Selector relief3_sel = Selector{"jot/relief", {
                {"$in", image3_sel.to_json()},
                {"width", 15.0},
                {"breadth", 15.0},
                {"height", 3.0},
                {"base", 1.0}
            }}.with_output("$out");

            std::cout << "    - Executing ReliefOp on valid heightmap..." << std::endl;
            Processor::execute(&vfs, relief3_sel);

            Shape relief_res = vfs.read<Shape>(relief3_sel);
            assert(relief_res.geometry.has_value());
            Geometry relief_geo = vfs.read<Geometry>(relief_res.geometry.value());

            std::cout << "    - Real Relief Vertices: " << relief_geo.vertices.size() << std::endl;
            std::cout << "    - Real Relief Triangles: " << relief_geo.triangles.size() << std::endl;

            // Assert that we generated a valid 3D solid base + top grid
            assert(relief_geo.vertices.size() > 0);

            vfs.verify_well_formed_solid(relief_geo, "Fetched Wikimedia Relief Mesh");

            // Generate verification PNG for Test Case 3
            Selector relief3_png_addr = Selector{"jot/png", {
                {"$in", relief3_sel.to_json()},
                {"ax", -0.61547}, 
                {"ay", 0.78539}
            }}.with_output("$out");

            std::cout << "    - Generating relief PNG (actual/relief_real_test_final.png)..." << std::endl;
            Processor::execute(&vfs, relief3_png_addr);
            
            std::vector<uint8_t> relief3_png_bytes = vfs.read<std::vector<uint8_t>>(relief3_png_addr);
            std::ofstream relief3_out("actual/relief_real_test_final.png", std::ios::binary);
            relief3_out.write((char*)relief3_png_bytes.data(), relief3_png_bytes.size());
            relief3_out.close();
            std::cout << "    - Saved real relief verification PNG to actual/relief_real_test_final.png" << std::endl;

            std::cout << "    - SUCCESS: Real heightmap relief generated successfully!" << std::endl;
        } catch (const std::exception& e) {
            std::string msg = e.what();
            if (msg.find("Failed to download") != std::string::npos || msg.find("Status:") != std::string::npos) {
                std::cout << "⚠️ Network/Rate-limiting error: " << e.what() << std::endl;
                std::cout << "⚠️ Skipping Test Case 3 due to unreachable Wikimedia server." << std::endl;
            } else {
                std::cerr << "❌ FAIL: Fetch or ReliefOp failed on valid heightmap: " << e.what() << std::endl;
                return 1;
            }
        }

        // 7. Test Case 4: ReliefOp with close = false (open mesh)
        std::cout << "  - Executing Test Case 4 (open mesh relief)..." << std::endl;
        try {
            Selector relief_open_sel = Selector{"jot/relief", {
                {"$in", image_sel.to_json()},
                {"width", 10.0},
                {"breadth", 10.0},
                {"height", 2.0},
                {"base", 1.0},
                {"minArea", 0.0},
                {"close", false}
            }}.with_output("$out");

            Processor::execute(&vfs, relief_open_sel);
            Shape open_result = vfs.read<Shape>(relief_open_sel);
            Geometry open_geo = vfs.read<Geometry>(open_result.geometry.value());
            
            std::cout << "    - Open Relief Vertices: " << open_geo.vertices.size() << std::endl;
            std::cout << "    - Open Relief Triangles: " << open_geo.triangles.size() << std::endl;

            // An open mesh should not be closed
            boolean::Surface_mesh open_mesh = boolean::Engine::geometry_to_mesh(open_geo);
            assert(!CGAL::is_closed(open_mesh));
            std::cout << "    - SUCCESS: Open relief mesh verified as not closed!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "❌ FAIL: Test Case 4 (close = false) failed: " << e.what() << std::endl;
            return 1;
        }

        std::cout << "✅ Relief Test PASS" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Relief verification failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
