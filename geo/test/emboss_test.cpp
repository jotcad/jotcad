#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include <cmath>
#include <cassert>

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("emboss");
    register_all_ops(&vfs);

    std::cout << "Testing Emboss (Surface Corefinement) Operation..." << std::endl;

    // 1. Create a Box Subject: width=30, height=30, depth=10
    // Centered at [0,0,0], so top face is at Z = 5.0
    Selector subject_sel = Selector{"jot/Box", {{"width", 30.0}, {"height", 30.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, subject_sel);

    // 2. Create a Disk Pattern: diameter=10
    Selector pattern_sel = Selector{"jot/Disk", {{"diameter", 10.0}, {"zag", 0.5}}}.with_output("$out");
    Processor::execute(&vfs, pattern_sel);

    // 3. Execute EmbossOp: depth=2.0, offset=-0.1
    // The pattern is projected onto the top surface (Z = 5.0).
    // Expected max Z of the embossed region = 5.0 - 0.1 + 2.0 = 6.9.
    Selector emboss_sel = Selector{"jot/emboss", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"depth", 2.0},
        {"offset", -0.1}
    }}.with_output("$out");

    std::cout << "  - Executing EmbossOp..." << std::endl;
    try {
        Processor::execute(&vfs, emboss_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ EmbossOp execution failed: " << e.what() << std::endl;
        return 1;
    }

    // 4. Verify results
    try {
        Shape result = vfs.read<Shape>(emboss_sel);
        if (!result.geometry.has_value()) {
            std::cerr << "❌ Emboss result does not contain geometry" << std::endl;
            return 1;
        }

        Geometry geo = vfs.read<Geometry>(result.geometry.value());
        std::cout << "  - Output vertices count: " << geo.vertices.size() << std::endl;
        std::cout << "  - Output triangles count: " << geo.triangles.size() << std::endl;

        double max_z = -1e18;
        for (const auto& v : geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_z) max_z = vz;
        }

        std::cout << "  - Max Z detected: " << max_z << " (Expected ~6.9)" << std::endl;
        
        // Assert that the max height reaches the correct embossed displacement level
        assert(max_z > 6.85 && max_z < 6.95);

        // 5. Render verification PNG
        Selector png_addr = Selector{"jot/png", {
            {"$in", emboss_sel.to_json()},
            {"ax", 0.61547}, 
            {"ay", 0.78539}
        }}.with_output("$out");

        std::cout << "  - Generating verification PNG (actual/emboss_test_final.png)..." << std::endl;
        Processor::execute(&vfs, png_addr);
        
        std::vector<uint8_t> png_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/emboss_test_final.png", std::ios::binary);
        out.write((char*)png_bytes.data(), png_bytes.size());
        out.close();
        std::cout << "  - Saved verification PNG to actual/emboss_test_final.png" << std::endl;

        std::cout << "✅ Emboss Corefinement Test PASS" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Emboss verification failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
