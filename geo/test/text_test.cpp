#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("text");
    register_all_ops(&vfs);

    std::cout << "Testing Text (Hole Triangulation) Operation..." << std::endl;

    // 1. Load local test font offline
    std::vector<std::string> search_paths = {
        "node_modules/three/examples/fonts/ttf/kenpixel.ttf",
        "../../node_modules/three/examples/fonts/ttf/kenpixel.ttf",
        "../../../node_modules/three/examples/fonts/ttf/kenpixel.ttf"
    };
    std::ifstream local_font;
    for (const auto& p : search_paths) {
        local_font.open(p, std::ios::binary);
        if (local_font.is_open()) break;
    }
    if (!local_font.is_open()) {
        std::cerr << "❌ Failed to open local test font" << std::endl;
        return 1;
    }
    std::vector<uint8_t> font_bytes((std::istreambuf_iterator<char>(local_font)), std::istreambuf_iterator<char>());
    CID font_cid = vfs.materialize(font_bytes);

    // 2. Generate text using the Font CID
    Selector text_addr = Selector{"jot/Text", {
        {"text", "B"},
        {"font", font_cid.value},
        {"size", 50.0}
    }}.with_output("$out");

    std::cout << "  - Executing TextOp (Triangulating with Identity)..." << std::endl;
    Processor::execute(&vfs, text_addr);

    // 3. Render to PNG for visual verification
    Selector png_addr = Selector{"jot/png", {
        {"$in", text_addr},
        {"ax", 0.0}, 
        {"ay", 0.0}
    }}.with_output("$out");

    std::cout << "  - Generating PNG..." << std::endl;
    Processor::execute(&vfs, png_addr);

    // 4. Verify results
    try {
        std::vector<uint8_t> png_file_bytes = vfs.read<std::vector<uint8_t>>(png_addr);
        std::string actual_hash = vfs_hash256(png_file_bytes);
        
        std::cout << "  - PNG generated OK (" << png_file_bytes.size() << " bytes)" << std::endl;
        std::cout << "  - SHA256: " << actual_hash << std::endl;
        
        std::filesystem::create_directories("actual");
        std::string filename = "actual/text_hole_test.png";
        std::ofstream out(filename, std::ios::binary);
        out.write((char*)png_file_bytes.data(), png_file_bytes.size());
        out.close();

        assert(png_file_bytes.size() > 1000);
        assert(png_file_bytes[0] == 0x89 && png_file_bytes[1] == 'P' && png_file_bytes[2] == 'N' && png_file_bytes[3] == 'G');

        std::cout << "✅ Text Hole Test Complete" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Text Hole Test FAILED: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
