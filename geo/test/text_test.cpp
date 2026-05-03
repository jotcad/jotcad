#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("text");
    register_all_ops(&vfs);

    std::cout << "Testing Text (Hole Triangulation) Operation..." << std::endl;

    // 1. Fetch a font
    // Use the direct raw.githubusercontent.com URL to avoid 302 redirects.
    std::string font_url = "https://raw.githubusercontent.com/google/fonts/main/ofl/greatvibes/GreatVibes-Regular.ttf";
    
    // 2. Generate text with a hole (e.g., 'B' or 'O')
    Selector text_addr = Selector{"jot/text", {
        {"text", "B"},
        {"font", font_url},
        {"size", 50.0}
    }}.with_output("$out");

    std::cout << "  - Executing TextOp (Downloading font + Triangulating)..." << std::endl;
    // This will now throw if the download fails, aborting the test as requested.
    Processor::execute(&vfs, text_addr);

    // 3. Render to PNG for visual verification
    // Top-down view (Vertical Default)
    Selector png_addr = Selector{"jot/png", {
        {"$in", text_addr},
        {"ax", 0.0}, 
        {"ay", 0.0}
    }}.with_output("$out");

    std::cout << "  - Generating PNG..." << std::endl;
    Processor::execute(&vfs, png_addr);

    // 4. Verify 'file' port and hash
    try {
        std::vector<uint8_t> png_file_bytes = vfs.read<std::vector<uint8_t>>(png_addr.with_output("file"));
        std::string actual_hash = vfs_hash256(png_file_bytes);
        
        std::cout << "  - 'file' port read OK (" << png_file_bytes.size() << " bytes)" << std::endl;
        std::cout << "  - SHA256: " << actual_hash << std::endl;
        
        // Save to actual/ for inspection
        std::filesystem::create_directories("actual");
        std::string filename = "actual/text_hole_test.png";
        std::ofstream out(filename, std::ios::binary);
        out.write((char*)png_file_bytes.data(), png_file_bytes.size());
        out.close();
        std::cout << "  - Saved actual image to " << filename << std::endl;

        // Baseline comparison (to be filled after first successful run)
        // For now, we'll just ensure it's a valid PNG and non-empty.
        assert(png_file_bytes.size() > 1000);
        assert(png_file_bytes[0] == 0x89 && png_file_bytes[1] == 'P' && png_file_bytes[2] == 'N' && png_file_bytes[3] == 'G');

        std::cout << "✅ Text Hole Test Complete" << std::endl;
        std::cout << "   MANUAL ACTION: Inspect " << filename << " to ensure holes in 'B' are empty." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Text Hole Test FAILED: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
