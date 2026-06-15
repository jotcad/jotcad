#include "test_base.h"
#include "../../fs/cpp/cid.h"
#include <cmath>
#include <cassert>
#include <fstream>

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

    // 3. Create flat relief shape with height = 1.9 (to get expected max Z of 6.9)
    Selector relief1_sel = Selector{"jot/Box", {
        {"width", 10.0},
        {"height", 10.0},
        {"depth", json::array({0.0, 1.9})}
    }}.with_output("$out");
    Processor::execute(&vfs, relief1_sel);

    // 4. Execute EmbossOp
    Selector emboss_sel = Selector{"jot/emboss", {
        {"$in", subject_sel.to_json()},
        {"pattern", pattern_sel.to_json()},
        {"relief", relief1_sel.to_json()}
    }}.with_output("$out");

    std::cout << "  - Executing EmbossOp (Test Case 1: Flat Relief)..." << std::endl;
    try {
        Processor::execute(&vfs, emboss_sel);
    } catch (const std::exception& e) {
        std::cerr << "❌ EmbossOp execution failed: " << e.what() << std::endl;
        return 1;
    }

    // 5. Verify results for Test Case 1
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

        // Render verification PNG for Test Case 1
        Selector png_addr = Selector{"jot/png", {
            {"$in", emboss_sel.to_json()},
            {"ax", -0.61547}, 
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

    } catch (const std::exception& e) {
        std::cerr << "❌ Emboss verification failed: " << e.what() << std::endl;
        return 1;
    }

    // 6. Execute EmbossOp (Test Case 2: Shifted Relief)
    std::cout << "  - Executing EmbossOp (Test Case 2: Shifted Relief)..." << std::endl;
    try {
        // Create relief shape Box of size 10x10x3 with Z from 0.0 to 3.0
        Selector relief2_sel = Selector{"jot/Box", {
            {"width", 10.0},
            {"height", 10.0},
            {"depth", json::array({0.0, 3.0})}
        }}.with_output("$out");
        Processor::execute(&vfs, relief2_sel);

        // Read the relief shape from VFS and apply Z-translation of -0.1
        Shape relief2_shape = vfs.read<Shape>(relief2_sel);
        relief2_shape.tf = Matrix::translate(0.0, 0.0, -0.1);

        // Write the shifted relief shape back to the VFS under a shifted selector address
        Selector relief2_shifted_sel = Selector{"jot/temp_relief_shifted", {}}.with_output("$out");
        vfs.write(relief2_shifted_sel, relief2_shape);

        // Emboss using the shifted relief shape
        // Max Z of the embossed shape should be: 5.0 (subject surface) + 2.9 (relief top face in world space) = 7.9
        Selector emboss_relief_sel = Selector{"jot/emboss", {
            {"$in", subject_sel.to_json()},
            {"pattern", pattern_sel.to_json()},
            {"relief", relief2_shifted_sel.to_json()}
        }}.with_output("$out");

        Processor::execute(&vfs, emboss_relief_sel);

        // Verify results for Test Case 2
        Shape relief_result = vfs.read<Shape>(emboss_relief_sel);
        assert(relief_result.geometry.has_value());

        Geometry relief_geo = vfs.read<Geometry>(relief_result.geometry.value());
        double max_relief_z = -1e18;
        for (const auto& v : relief_geo.vertices) {
            double vz = CGAL::to_double(v.z);
            if (vz > max_relief_z) max_relief_z = vz;
        }

        std::cout << "  - Relief Emboss Max Z detected: " << max_relief_z << " (Expected ~7.9)" << std::endl;
        
        // Assert that the max height reaches the correct relief-mapped displacement level
        assert(max_relief_z > 7.85 && max_relief_z < 7.95);

        // Generate verification PNG for Test Case 2
        Selector relief_png_addr = Selector{"jot/png", {
            {"$in", emboss_relief_sel.to_json()},
            {"ax", -0.61547}, 
            {"ay", 0.78539}
        }}.with_output("$out");

        std::cout << "  - Generating relief emboss PNG (actual/emboss_relief_test_final.png)..." << std::endl;
        Processor::execute(&vfs, relief_png_addr);
        
        std::vector<uint8_t> relief_png_bytes = vfs.read<std::vector<uint8_t>>(relief_png_addr);
        std::ofstream relief_out("actual/emboss_relief_test_final.png", std::ios::binary);
        relief_out.write((char*)relief_png_bytes.data(), relief_png_bytes.size());
        relief_out.close();
        std::cout << "  - Saved relief emboss verification PNG to actual/emboss_relief_test_final.png" << std::endl;

        // 7. Execute EmbossOp (Test Case 3: Brickwork 3D Texture on Cube)
        std::cout << "  - Executing EmbossOp (Test Case 3: Brickwork 3D Texture on Cube)..." << std::endl;
        try {
            // Create Cube Subject: 10x10x10
            Selector subject_cube_sel = Selector{"jot/Box", {
                {"width", 10.0},
                {"height", 10.0},
                {"depth", 10.0}
            }}.with_output("$out");
            Processor::execute(&vfs, subject_cube_sel);

            // Create Disk Pattern: diameter = 12
            Selector pattern_disk_sel = Selector{"jot/Disk", {
                {"diameter", 12.0},
                {"zag", 0.5}
            }}.with_output("$out");
            Processor::execute(&vfs, pattern_disk_sel);

            // Create 4 Brick Boxes as compound relief
            Selector b1_sel = Selector{"jot/Box", {
                {"width", json::array({-5.0, -0.5})},
                {"height", json::array({0.5, 5.0})},
                {"depth", json::array({0.0, 1.5})}
            }}.with_output("$out");
            Processor::execute(&vfs, b1_sel);
            Shape b1 = vfs.read<Shape>(b1_sel);

            Selector b2_sel = Selector{"jot/Box", {
                {"width", json::array({0.5, 5.0})},
                {"height", json::array({0.5, 5.0})},
                {"depth", json::array({0.0, 1.5})}
            }}.with_output("$out");
            Processor::execute(&vfs, b2_sel);
            Shape b2 = vfs.read<Shape>(b2_sel);

            Selector b3_sel = Selector{"jot/Box", {
                {"width", json::array({-5.0, -0.5})},
                {"height", json::array({-5.0, -0.5})},
                {"depth", json::array({0.0, 1.5})}
            }}.with_output("$out");
            Processor::execute(&vfs, b3_sel);
            Shape b3 = vfs.read<Shape>(b3_sel);

            Selector b4_sel = Selector{"jot/Box", {
                {"width", json::array({0.5, 5.0})},
                {"height", json::array({-5.0, -0.5})},
                {"depth", json::array({0.0, 1.5})}
            }}.with_output("$out");
            Processor::execute(&vfs, b4_sel);
            Shape b4 = vfs.read<Shape>(b4_sel);

            // Group the bricks together
            Shape brickwork_group = Shape::group({b1, b2, b3, b4});
            brickwork_group.add_tag("type", "group");

            Selector brickwork_sel = Selector{"jot/temp_brickwork_relief", {}}.with_output("$out");
            vfs.write(brickwork_sel, brickwork_group);

            // Run Emboss Operation
            Selector emboss_brick_sel = Selector{"jot/emboss", {
                {"$in", subject_cube_sel.to_json()},
                {"pattern", pattern_disk_sel.to_json()},
                {"relief", brickwork_sel.to_json()}
            }}.with_output("$out");
            Processor::execute(&vfs, emboss_brick_sel);

            // Verify Test Case 3 Results
            Shape brick_result = vfs.read<Shape>(emboss_brick_sel);
            assert(brick_result.geometry.has_value());

            Geometry brick_geo = vfs.read<Geometry>(brick_result.geometry.value());
            
            // Assert watertightness and volume correctness
            vfs.verify_well_formed_solid(brick_geo, "Embossed Brickwork Cube");

            // Inspect the bounding box of the displaced geometry
            double min_x = 1e18, max_x = -1e18;
            double min_y = 1e18, max_y = -1e18;
            double min_z = 1e18, max_z = -1e18;
            for (const auto& v : brick_geo.vertices) {
                double vx = CGAL::to_double(v.x);
                double vy = CGAL::to_double(v.y);
                double vz = CGAL::to_double(v.z);
                if (vx < min_x) min_x = vx;
                if (vx > max_x) max_x = vx;
                if (vy < min_y) min_y = vy;
                if (vy > max_y) max_y = vy;
                if (vz < min_z) min_z = vz;
                if (vz > max_z) max_z = vz;
            }

            std::cout << "  - Brickwork Emboss Bounding Box:" << std::endl;
            std::cout << "    X: [" << min_x << ", " << max_x << "] (Expected ~[-6.5, 6.5])" << std::endl;
            std::cout << "    Y: [" << min_y << ", " << max_y << "] (Expected ~[-6.5, 6.5])" << std::endl;
            std::cout << "    Z: [" << min_z << ", " << max_z << "] (Expected ~[-6.5, 6.5])" << std::endl;

            // Since the brick thickness is 1.5, displacement along the 6 face normals of the 10x10x10 cube 
            // will extend each side from 5.0 to 6.5 (or -5.0 to -6.5).
            assert(max_x > 6.45 && max_x < 6.55);
            assert(min_x > -6.55 && min_x < -6.45);
            assert(max_y > 6.45 && max_y < 6.55);
            assert(min_y > -6.55 && min_y < -6.45);
            assert(max_z > 6.45 && max_z < 6.55);
            assert(min_z > -6.55 && min_z < -6.45);

            // Generate verification PNG for Test Case 3
            Selector brick_png_addr = Selector{"jot/png", {
                {"$in", emboss_brick_sel.to_json()},
                {"ax", -0.61547}, 
                {"ay", 0.78539}
            }}.with_output("$out");

            std::cout << "  - Generating brickwork emboss PNG (actual/emboss_brick_test_final.png)..." << std::endl;
            Processor::execute(&vfs, brick_png_addr);
            
            std::vector<uint8_t> brick_png_bytes = vfs.read<std::vector<uint8_t>>(brick_png_addr);
            std::ofstream brick_out("actual/emboss_brick_test_final.png", std::ios::binary);
            brick_out.write((char*)brick_png_bytes.data(), brick_png_bytes.size());
            brick_out.close();
            std::cout << "  - Saved brickwork emboss verification PNG to actual/emboss_brick_test_final.png" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "❌ Emboss brickwork test failed: " << e.what() << std::endl;
            return 1;
        }

        std::cout << "✅ Emboss Corefinement Test PASS" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Emboss relief test failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
