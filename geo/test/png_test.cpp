#include "png_op.h"
#include "box_op.h"
#include "impl/protocols.h"
#include "impl/processor.h"
#include "../../fs/cpp/include/cid.h"
#include <iostream>
#include <cassert>
#include <filesystem>

using namespace jotcad::geo;
using namespace fs;

int main() {
    std::cout << "Testing PNG (Partitioned Port) Operation..." << std::endl;

    VFSNode::Config config;
    config.id = "png-test-node";
    config.storage_dir = ".vfs_storage_png_test";
    std::filesystem::remove_all(config.storage_dir);
    
    VFSNode vfs(config);

    // Register Box and PNG
    box_init();
    png_init();

    // 1. Create a box
    Selector box_addr = {"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}};
    BoxOp<>::execute(&vfs, box_addr, 10.0, 10.0, 10.0);

    // 2. Run PNG on the box
    Selector png_addr = {"jot/png", {{"$in", box_addr}}};
    PngOp<>::execute(&vfs, png_addr, vfs.read<Shape>(box_addr));

    // 3. Verify 'file' port
    try {
        std::vector<uint8_t> png_file_bytes = vfs.read<std::vector<uint8_t>>(png_addr, "file");
        std::string actual_hash = vfs_hash256(png_file_bytes);
        std::string golden_hash = "e75568a0ed20cf3fda2d349cf6bcd3cf3f692984e26ff3a1adc867b369d581f3";
        
        std::cout << "  - 'file' port read OK (" << png_file_bytes.size() << " bytes)" << std::endl;
        std::cout << "  - SHA256: " << actual_hash << std::endl;
        
        if (actual_hash != golden_hash) {
            std::cerr << "❌ PNG FAIL: Hash mismatch!" << std::endl;
            std::cerr << "   Expected: " << golden_hash << std::endl;
            std::cerr << "   Actual:   " << actual_hash << std::endl;
            return 1;
        }
        
        assert(png_file_bytes.size() > 100);
        assert(png_file_bytes[0] == 0x89 && png_file_bytes[1] == 'P' && png_file_bytes[2] == 'N' && png_file_bytes[3] == 'G');
        
        // Write to a local file for manual inspection/baselining
        std::filesystem::create_directories("baselines");
        std::ofstream out("baselines/box_test.png", std::ios::binary);
        out.write((char*)png_file_bytes.data(), png_file_bytes.size());
        std::cout << "  - Saved to baselines/box_test.png" << std::endl;

        std::cout << "✅ PNG Port Test PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ PNG Port Test FAILED: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
