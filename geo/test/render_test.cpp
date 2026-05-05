#include "protocols.h"
#include "processor.h"
#include "rasterizer.h"
#include "box_op.h"
#include "hexagon_op.h"
#include "triangle_op.h"
#include "../../fs/cpp/cid.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <map>

using namespace jotcad::geo;
using namespace fs;

struct TestResult {
    bool passed;
    std::string actual_hash;
};

TestResult run_render_test(VFSNode* vfs, const std::string& name, const Shape& shape, const std::string& expected_hash) {
    std::cout << "Testing " << name << "..." << std::endl;
    
    try {
        Geometry geo = vfs->read<Geometry>(shape.geometry.value());
        std::vector<uint8_t> png_bytes = Rasterizer::render_png(geo, 256, 256);
        
        std::string actual_hash = vfs_hash256(png_bytes);
        
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/" + name + ".png", std::ios::binary);
        out.write((char*)png_bytes.data(), png_bytes.size());
        out.close();

        if (actual_hash != expected_hash) {
            std::cout << "  ❌ FAILED" << std::endl;
            std::cout << "     Expected: " << expected_hash << std::endl;
            std::cout << "     Actual:   " << actual_hash << std::endl;
            return {false, actual_hash};
        }

        std::cout << "  ✅ PASSED" << std::endl;
        return {true, actual_hash};
    } catch (const std::exception& e) {
        std::cout << "  ❌ ERROR: " << e.what() << std::endl;
        return {false, ""};
    }
}

int main() {
    VFSNode::Config config;
    config.id = "render-test-node";
    config.storage_dir = ".vfs_storage_render_test";
    std::filesystem::remove_all(config.storage_dir);
    VFSNode vfs(config);

    // Initialize Ops
    box_init();
    hexagon_init();
    triangle_init();

    std::map<std::string, std::pair<Shape, std::string>> tests;

    // 1. Box
    Selector box_addr = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    Processor::execute(&vfs, box_addr);
    tests["box"] = {vfs.read<Shape>(box_addr), "95e7a44b7954f9faa3839b5fee1c67161510081c5d472562b742a4876105f06b"};

    // 2. Hexagon
    Selector hex_addr = {"jot/Hexagon/diameter", {{"diameter", 10.0}}};
    Processor::execute(&vfs, hex_addr);
    tests["hexagon"] = {vfs.read<Shape>(hex_addr), ""}; // Needs initial baseline

    // 3. Triangle
    Selector tri_addr = {"jot/Triangle/equilateral", {{"size", {10.0}}}} ;
    Processor::execute(&vfs, tri_addr);
    tests["triangle"] = {vfs.read<Shape>(tri_addr.with_output("$out")), ""}; // Needs initial baseline

    bool all_passed = true;
    for (auto const& [name, data] : tests) {
        TestResult res = run_render_test(&vfs, name, data.first, data.second);
        if (!res.passed) all_passed = false;
    }

    return all_passed ? 0 : 1;
}
