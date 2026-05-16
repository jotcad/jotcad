#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("png");
    register_all_ops(&vfs);

    std::cout << "Testing PNG (Partitioned Port) Operation..." << std::endl;

    // 1. Create a box
    Selector box_addr = Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);

    // 2. Run PNG on the box with explicit isometric angles for golden comparison
    Selector png_addr = Selector{"jot/png", {{"$in", box_addr}, {"ax", 0.61547}, {"ay", 0.78539}}}.with_output("$out");
    Processor::execute(&vfs, png_addr);

    // 3. Verify '$out' port has binary PNG data
    try {
        std::vector<uint8_t> png_file_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::string actual_hash = vfs_hash256(png_file_bytes);
        std::string golden_hash = "6bd8fd0d23d90f749db33bd8153e1cf11bd2983f88c3abaca70bb2728b99a1c9";
        
        std::cout << "  - 'file' port read OK (" << png_file_bytes.size() << " bytes)" << std::endl;
        std::cout << "  - SHA256: " << actual_hash << std::endl;
        
        // Always write the actual output for inspection
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/box_test.png", std::ios::binary);
        out.write((char*)png_file_bytes.data(), png_file_bytes.size());
        out.close();
        std::cout << "  - Saved observed image to actual/box_test.png" << std::endl;

        if (actual_hash != golden_hash) {
            std::cerr << "❌ PNG FAIL: Hash mismatch!" << std::endl;
            std::cerr << "   Expected: " << golden_hash << std::endl;
            std::cerr << "   Actual:   " << actual_hash << std::endl;
            return 1;
        }
        
        assert(png_file_bytes.size() > 100);
        assert(png_file_bytes[0] == 0x89 && png_file_bytes[1] == 'P' && png_file_bytes[2] == 'N' && png_file_bytes[3] == 'G');
        
        std::cout << "✅ PNG Port Test PASS" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ PNG Port Test FAILED: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
