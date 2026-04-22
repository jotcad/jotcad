#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include "impl/protocols.h"
#include "vfs_node.h"
#include "impl/processor.h"
#include "rule_op.h"

using namespace jotcad;
using namespace jotcad::geo;
using namespace fs;

int main() {
    VFSNode::Config cfg;
    cfg.id = "rule-test-node";
    cfg.storage_dir = ".vfs_storage_rule_test";
    // Ensure clean state
    std::filesystem::remove_all(cfg.storage_dir);
    VFSNode vfs(cfg);
    
    // 1. Register the op so the VFS knows how to execute "jot/Rule"
    Processor::register_op<RuleOp<>, Shape, Shape>("jot/Rule");

    // Helper to create a simple open polyline (using segments)
    auto make_polyline = [&](double y_offset) {
        Geometry geo;
        geo.vertices.push_back({FT(0.0), FT(y_offset), FT(0.0)});
        geo.vertices.push_back({FT(10.0), FT(y_offset + 2.0), FT(0.0)}); // Slight curve
        geo.vertices.push_back({FT(20.0), FT(y_offset), FT(0.0)});
        geo.segments.push_back({0, 1});
        geo.segments.push_back({1, 2});
        Shape s;
        s.geometry = vfs.write_anonymous<Geometry>(geo);
        return s;
    };

    // Helper to create a simple closed loop (using faces)
    auto make_loop = [&](double z_offset, double size) {
        Geometry geo;
        geo.vertices.push_back({FT(0.0), FT(0.0), FT(z_offset)});
        geo.vertices.push_back({FT(size), FT(0.0), FT(z_offset)});
        geo.vertices.push_back({FT(size), FT(size), FT(z_offset)});
        geo.vertices.push_back({FT(0.0), FT(size), FT(z_offset)});
        Geometry::Face f;
        f.loops.push_back({0, 1, 2, 3});
        geo.faces.push_back(f);
        Shape s;
        s.geometry = vfs.write_anonymous<Geometry>(geo);
        return s;
    };

    auto start = std::chrono::high_resolution_clock::now();

    // Test 1: Polyline to Polyline
    std::cout << "[Test 1] Polyline to Polyline..." << std::endl;
    Shape p1 = make_polyline(0.0);
    Shape p2 = make_polyline(10.0);
    
    // IMPORTANT: In the VFS, we don't just "read", we must fulfill the address.
    // RuleOp::execute is the "Actor" that satisfies the contract.
    Selector sel1{"jot/Rule", {{"$a", p1}, {"$b", p2}}, "$out"};
    try {
        RuleOp<>::execute(&vfs, sel1, p1, p2); 
        
        Shape res1 = vfs.read<Shape>(sel1);
        if (res1.geometry.has_value()) {
            Geometry geo1 = vfs.read<Geometry>(res1.geometry.value());
            std::cout << "  ✅ SUCCESS: Resulting faces: " << geo1.faces.size() << " (Expected ~4)" << std::endl;
        } else {
            std::cout << "  ❌ FAILED: No geometry returned." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ❌ ERROR: " << e.what() << std::endl;
    }

    // Test 2: Loop to Loop (The "Lofted Box" case)
    std::cout << "[Test 2] Loop to Loop..." << std::endl;
    Shape l1 = make_loop(0.0, 10.0);
    Shape l2 = make_loop(10.0, 15.0); // Tapered
    Selector sel2{"jot/Rule", {{"$a", l1}, {"$b", l2}}, "$out"};
    try {
        RuleOp<>::execute(&vfs, sel2, l1, l2);
        
        Shape res2 = vfs.read<Shape>(sel2);
        if (res2.geometry.has_value()) {
            Geometry geo2 = vfs.read<Geometry>(res2.geometry.value());
            std::cout << "  ✅ SUCCESS: Resulting faces: " << geo2.faces.size() << " (Expected 8)" << std::endl;
        } else {
            std::cout << "  ❌ FAILED: No geometry returned." << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ❌ ERROR: " << e.what() << std::endl;
    }

    // Test 3: Multiple Loops (Holes)
    // We create a "Frame" (Square with a Square hole) and rule it to another.
    std::cout << "[Test 3] Multiple Loops (Holes)..." << std::endl;
    Geometry frame_geo;
    // Outer
    frame_geo.vertices.push_back({FT(0), FT(0), FT(0)});
    frame_geo.vertices.push_back({FT(100), FT(0), FT(0)});
    frame_geo.vertices.push_back({FT(100), FT(100), FT(0)});
    frame_geo.vertices.push_back({FT(0), FT(100), FT(0)});
    // Inner (Hole)
    frame_geo.vertices.push_back({FT(40), FT(40), FT(0)});
    frame_geo.vertices.push_back({FT(60), FT(40), FT(0)});
    frame_geo.vertices.push_back({FT(60), FT(60), FT(0)});
    frame_geo.vertices.push_back({FT(40), FT(60), FT(0)});
    
    Geometry::Face frame_face;
    frame_face.loops.push_back({0, 1, 2, 3});    // Outer
    frame_face.loops.push_back({4, 5, 6, 7});    // Hole
    frame_geo.faces.push_back(frame_face);
    
    Shape s_frame_1;
    s_frame_1.geometry = vfs.write_anonymous<Geometry>(frame_geo);
    
    Shape s_frame_2 = s_frame_1;
    s_frame_2.tf = Matrix::translate(0, 0, 50).to_vec();
    
    Selector sel3{"jot/Rule", {{"$a", s_frame_1}, {"$b", s_frame_2}}, "$out"};
    try {
        RuleOp<>::execute(&vfs, sel3, s_frame_1, s_frame_2);
        Shape res3 = vfs.read<Shape>(sel3);
        if (res3.geometry.has_value()) {
            Geometry geo3 = vfs.read<Geometry>(res3.geometry.value());
            // Expected: 8 faces for outer wall + 8 faces for inner wall = 16
            std::cout << "  ✅ SUCCESS: Resulting faces: " << geo3.faces.size() << " (Expected 16)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ❌ ERROR: " << e.what() << std::endl;
    }

    // Test 4: Implicit loops (Point components)
    std::cout << "[Test 4] Implicit Loops (Point components)..." << std::endl;
    Shape g1, g2;
    for (int i = 0; i < 4; ++i) {
        Shape pt1, pt2;
        double a = i * 90.0 * M_PI / 180.0;
        pt1.tf = Matrix::translate(10 * std::cos(a), 10 * std::sin(a), 0).to_vec();
        pt2.tf = Matrix::translate(15 * std::cos(a), 15 * std::sin(a), 50).to_vec();
        g1.components.push_back(pt1);
        g2.components.push_back(pt2);
    }
    
    Selector sel4{"jot/Rule", {{"$a", g1}, {"$b", g2}}, "$out"};
    try {
        RuleOp<>::execute(&vfs, sel4, g1, g2);
        Shape res4 = vfs.read<Shape>(sel4);
        if (res4.geometry.has_value()) {
            Geometry geo4 = vfs.read<Geometry>(res4.geometry.value());
            std::cout << "  ✅ SUCCESS: Resulting faces: " << geo4.faces.size() << " (Expected 8)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "  ❌ ERROR: " << e.what() << std::endl;
    }

    // Test 5: Stress Test (High vertex count)
    std::cout << "[Test 5] Stress Test (100 vertices per loop)..." << std::endl;
    Shape s_high_1, s_high_2;
    Geometry high_geo_1, high_geo_2;
    for (int i = 0; i < 100; ++i) {
        double a = (i / 100.0) * 2.0 * M_PI;
        high_geo_1.vertices.push_back({FT(50 * std::cos(a)), FT(50 * std::sin(a)), FT(0)});
        high_geo_2.vertices.push_back({FT(50 * std::cos(a)), FT(50 * std::sin(a)), FT(100)});
    }
    Geometry::Face f_high;
    std::vector<int> l_high;
    for (int i = 0; i < 100; ++i) l_high.push_back(i);
    f_high.loops.push_back(l_high);
    high_geo_1.faces.push_back(f_high);
    high_geo_2.faces.push_back(f_high);
    
    s_high_1.geometry = vfs.write_anonymous<Geometry>(high_geo_1);
    s_high_2.geometry = vfs.write_anonymous<Geometry>(high_geo_2);
    
    auto stress_start = std::chrono::high_resolution_clock::now();
    Selector sel5{"jot/Rule", {{"$a", s_high_1}, {"$b", s_high_2}}, "$out"};
    RuleOp<>::execute(&vfs, sel5, s_high_1, s_high_2);
    auto stress_end = std::chrono::high_resolution_clock::now();
    
    Shape res5 = vfs.read<Shape>(sel5);
    if (res5.geometry.has_value()) {
        Geometry geo5 = vfs.read<Geometry>(res5.geometry.value());
        std::cout << "  ✅ SUCCESS: Resulting faces: " << geo5.faces.size() << " (Expected 200)" << std::endl;
        std::cout << "  ⏱️  Triangulation Latency: " << std::chrono::duration_cast<std::chrono::microseconds>(stress_end - stress_start).count() << " us" << std::endl;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "All tests completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms." << std::endl;

    // Clean exit to avoid static destruction race
    std::cout << "Cleaning up..." << std::endl;
    return 0;
}
