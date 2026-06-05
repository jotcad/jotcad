#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include <fstream>
#include <vector>
#include <functional>

extern "C" unsigned char *stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

using namespace jotcad::geo;
using namespace fs;

// Helper function to dynamically construct a cyan background shape behind a traced shape
Shape make_cyan_background_for_shape(MockVFS& vfs, const Shape& s) {
    double min_x = 1e9, max_x = -1e9, min_y = 1e9, max_y = -1e9;
    std::function<void(const Shape&)> traverse = [&](const Shape& sh) {
        if (sh.geometry.has_value()) {
            Geometry geo = vfs.read<Geometry>(sh.geometry.value());
            for (const auto& v : geo.vertices) {
                double x = CGAL::to_double(v.x);
                double y = CGAL::to_double(v.y);
                if (x < min_x) min_x = x;
                if (x > max_x) max_x = x;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
        for (const auto& c : sh.components) traverse(c);
    };
    traverse(s);

    if (min_x > max_x) {
        min_x = -10; max_x = 10; min_y = -10; max_y = 10;
    }

    // Add padding to ensure the cyan box is visible around the edges
    double pad = 5.0;
    min_x -= pad; max_x += pad; min_y -= pad; max_y += pad;

    Geometry bg_geo;
    bg_geo.vertices.push_back({FT(min_x), FT(min_y), FT(-0.1)});
    bg_geo.vertices.push_back({FT(max_x), FT(min_y), FT(-0.1)});
    bg_geo.vertices.push_back({FT(max_x), FT(max_y), FT(-0.1)});
    bg_geo.vertices.push_back({FT(min_x), FT(max_y), FT(-0.1)});
    bg_geo.faces.push_back({{{0, 1, 2, 3}}});
    return JotVfsProtocol::make_shape(&vfs, bg_geo, {{"color", "#00ffff"}, {"type", "surface"}});
}

int main() {
    MockVFS vfs("trace_quant");
    register_all_ops(&vfs);

    std::cout << "Testing Trace (Automatic Quantization) Operation..." << std::endl;

    // ==========================================
    // PHASE 1: Self-Contained Offline Mock Image Trace
    // ==========================================
    std::cout << "\n[Phase 1] Tracing offline 4-quadrant mock PNG..." << std::endl;
    const int W = 16, H = 16;
    unsigned char pixels[W * H * 3];
    for (int y = 0; y < H; ++y) {
        for (int x = 0; x < W; ++x) {
            int idx = (x + y * W) * 3;
            if (y < H / 2) {
                if (x < W / 2) {
                    // Red
                    pixels[idx] = 255; pixels[idx+1] = 0; pixels[idx+2] = 0;
                } else {
                    // Green
                    pixels[idx] = 0; pixels[idx+1] = 255; pixels[idx+2] = 0;
                }
            } else {
                if (x < W / 2) {
                    // Blue
                    pixels[idx] = 0; pixels[idx+1] = 0; pixels[idx+2] = 255;
                } else {
                    // White
                    pixels[idx] = 255; pixels[idx+1] = 255; pixels[idx+2] = 255;
                }
            }
        }
    }

    int len = 0;
    unsigned char* png_data = stbi_write_png_to_mem(pixels, W * 3, W, H, 3, &len);
    if (!png_data) {
        std::cerr << "❌ Failed to generate test PNG" << std::endl;
        return 1;
    }
    std::vector<uint8_t> png_bytes(png_data, png_data + len);
    free(png_data);

    // Write PNG bytes to VFS under a known selector/CID
    Selector img_addr = Selector{"jot/Image", {{"url", "mock://trace_test.png"}}}.with_output("$out");
    vfs.write(img_addr, png_bytes);
    std::cout << "  - Generated mock PNG of size " << png_bytes.size() << " bytes and wrote to VFS" << std::endl;

    // Execute TraceOp on mock image
    Selector trace_addr = Selector{"jot/trace", {
        {"$in", img_addr.to_json()},
        {"colors", 4},
        {"smooth", 0.0}, // Test raw pixel boundaries with no simplification
        {"minArea", 16.0}
    }}.with_output("$out");

    std::cout << "  - Executing TraceOp (HSV Quantization)..." << std::endl;
    try {
        Processor::execute(&vfs, trace_addr);
    } catch (const std::exception& e) {
        std::cerr << "❌ TraceOp failed: " << e.what() << std::endl;
        return 1;
    }

    // Verify mock results
    try {
        Shape result = vfs.read<Shape>(trace_addr);
        std::cout << "  - Number of color buckets: " << result.components.size() << std::endl;
        assert(result.components.size() == 4);
        
        // Add cyan background behind the trace
        Shape bg = make_cyan_background_for_shape(vfs, result);
        std::vector<Shape> combined_components;
        combined_components.push_back(bg);
        combined_components.insert(combined_components.end(), result.components.begin(), result.components.end());
        Shape combined = Shape::group(combined_components);

        Selector combined_addr = Selector{"user/combined_mock", {}}.with_output("$out");
        vfs.write(combined_addr, combined);

        // Render verification PNG
        Selector png_addr = Selector{"jot/png", {
            {"$in", combined_addr.to_json()},
            {"ax", 0.0}, 
            {"ay", 0.0}
        }}.with_output("$out");

        std::cout << "  - Generating verification PNG with Cyan background (actual/trace_test_final.png)..." << std::endl;
        Processor::execute(&vfs, png_addr);
        
        std::vector<uint8_t> final_png_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/trace_test_final.png", std::ios::binary);
        out.write((char*)final_png_bytes.data(), final_png_bytes.size());
        out.close();
        
        std::cout << "✅ Phase 1 Mock Trace Test Complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Verification failed: " << e.what() << std::endl;
        return 1;
    }

    // ==========================================
    // PHASE 2: Real Heightmap Image Trace Integration
    // ==========================================
    std::cout << "\n[Phase 2] Tracing real heightmap of Trencrom Hill from Wikimedia Commons..." << std::endl;
    std::string real_img_url = "https://upload.wikimedia.org/wikipedia/commons/c/c3/Heightmap_of_Trencrom_Hill.png";
    Selector real_img_addr = Selector{"jot/Image", {{"url", real_img_url}}}.with_output("$out");
    
    std::cout << "  - Fetching image: " << real_img_url << std::endl;
    try {
        Processor::execute(&vfs, real_img_addr);
    } catch (const std::exception& e) {
        std::cerr << "❌ Image fetch failed: " << e.what() << std::endl;
        return 1;
    }

    Selector real_trace_addr = Selector{"jot/trace", {
        {"$in", real_img_addr.to_json()},
        {"colors", 8},
        {"smooth", 0.0}, // Trace with no simplification for raw boundaries
        {"minArea", 25.0}
    }}.with_output("$out");

    std::cout << "  - Executing TraceOp on real image (8 colors)..." << std::endl;
    try {
        Processor::execute(&vfs, real_trace_addr);
    } catch (const std::exception& e) {
        std::cerr << "❌ TraceOp failed on real image: " << e.what() << std::endl;
        return 1;
    }

    try {
        Shape real_result = vfs.read<Shape>(real_trace_addr);
        std::cout << "  - Number of color buckets: " << real_result.components.size() << std::endl;

        // Add cyan background behind the trace
        Shape bg = make_cyan_background_for_shape(vfs, real_result);
        std::vector<Shape> combined_components;
        combined_components.push_back(bg);
        combined_components.insert(combined_components.end(), real_result.components.begin(), real_result.components.end());
        Shape combined = Shape::group(combined_components);

        Selector combined_real_addr = Selector{"user/combined_real", {}}.with_output("$out");
        vfs.write(combined_real_addr, combined);

        Selector png_real_addr = Selector{"jot/png", {
            {"$in", combined_real_addr.to_json()},
            {"ax", 0.0},
            {"ay", 0.0}
        }}.with_output("$out");

        std::cout << "  - Generating verification PNG with Cyan background (actual/trace_real_test_final.png)..." << std::endl;
        Processor::execute(&vfs, png_real_addr);

        std::vector<uint8_t> real_png_bytes = vfs.read<std::vector<uint8_t>>(png_real_addr);
        std::ofstream out("actual/trace_real_test_final.png", std::ios::binary);
        out.write((char*)real_png_bytes.data(), real_png_bytes.size());
        out.close();

        std::cout << "✅ Phase 2 Real Trace Test Complete." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Real trace verification failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
