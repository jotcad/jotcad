#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include "../../fs/cpp/vendor/stb_image_write.h"
#include "../render/rasterizer.h"
#include "../render/camera.h"
#include <cmath>
#include <cassert>
#include <fstream>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    std::cout << "Testing Decal (Robust Per-Patch Texturing) Operation..." << std::endl;
    MockVFS vfs("decal");
    register_all_ops(&vfs);

    // 1. Create a 30x30x10 subject box
    fs::Selector subject_sel = fs::Selector{"jot/Box", {{"width", 30.0}, {"height", 30.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, subject_sel);

    // 2. Create a relief image (1x1 white pixel)
    std::vector<uint8_t> png_bytes = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53,
        0xde, 0x00, 0x00, 0x00, 0x0c, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0xff, 0xff, 0x3f,
        0x00, 0x05, 0xfe, 0x02, 0xfe, 0xdc, 0x44, 0x74, 0x8e, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
        0x44, 0xae, 0x42, 0x60, 0x82
    };
    fs::Selector image_sel = fs::Selector{"jot/Image", {{"url", "mock://decal_test.png"}}}.with_output("$out");
    vfs.write(image_sel, png_bytes);

    // 3. Generate large open relief shape (100x100) to ensure full coverage of unfolded box
    fs::Selector relief_sel = fs::Selector{"jot/relief", {
        {"$in", image_sel.to_json()},
        {"width", 100.0},
        {"breadth", 100.0},
        {"height", 2.0},
        {"base", 0.0},
        {"minArea", 0.0},
        {"close", false}
    }}.with_output("$out");
    Processor::execute(&vfs, relief_sel);

    // 4. Test Case 1: Execute DecalOp
    fs::Selector decal_sel = fs::Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"relief", relief_sel.to_json()},
        {"seam", "skirting"}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp..." << std::endl;
    try {
        Processor::execute(&vfs, decal_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ DecalOp failed: " << e.what() << std::endl;
        return 1;
    }

    // Verify results
    try {
        Shape result = vfs.read<Shape>(decal_sel);
        assert(result.geometry.has_value());
        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        
        std::cout << "    * Output vertices count: " << geo.vertices.size() << std::endl;
        assert(geo.vertices.size() > 0);

        // Max height should be subject top (5.0) + relief height (2.0) = 7.0
        double max_z = -1e18;
        for (const auto& v : geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_z) max_z = vz;
        }

        std::cout << "    * Max Z detected: " << max_z << " (Expected ~7.0)" << std::endl;
        assert(max_z > 6.9 && max_z < 7.1);

    } catch (const std::exception& e) {
        std::cerr << "❌ Decal verification failed: " << e.what() << std::endl;
        return 1;
    }

    // 5. Test Case 2: Verify error on insufficient coverage
    fs::Selector tiny_relief_sel = fs::Selector{"jot/relief", {
        {"$in", image_sel.to_json()},
        {"width", 1.0},
        {"breadth", 1.0},
        {"height", 1.0},
        {"base", 0.0},
        {"close", false}
    }}.with_output("$out");
    Processor::execute(&vfs, tiny_relief_sel);

    fs::Selector decal_fail_sel = fs::Selector{"jot/decal", {
        {"$in", subject_sel.to_json()},
        {"relief", tiny_relief_sel.to_json()}
    }}.with_output("$out");

    std::cout << "  - Executing DecalOp (insufficient coverage)..." << std::endl;
    bool threw = false;
    try {
        Processor::execute(&vfs, decal_fail_sel);
    } catch (const std::exception& e) {
        std::cout << "    * Caught expected exception: " << e.what() << std::endl;
        if (std::string(e.what()).find("complete coverage") != std::string::npos) {
            threw = true;
        }
    }
    assert(threw);

    std::cout << "✅ Decal Test PASS" << std::endl;
    return 0;
}
