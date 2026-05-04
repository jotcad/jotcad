#include "test_base.h"
#include "../../fs/cpp/cid.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("trace_quant");
    register_all_ops(&vfs);

    std::cout << "Testing Trace (Automatic Quantization) Operation..." << std::endl;

    // 1. Fetch the Image via ImageOp
    // Using a reliable public URL for portability
    std::string img_url = "https://upload.wikimedia.org/wikipedia/commons/thumb/e/e0/SNice.svg/240px-SNice.svg.png";
    Selector img_addr = Selector{"jot/Image", {{"url", img_url}}}.with_output("$out");
    
    std::cout << "  - Fetching image: " << img_url << std::endl;
    try {
        Processor::execute(&vfs, img_addr);
    } catch (const std::exception& e) {
        std::cerr << "❌ Image fetch failed: " << e.what() << std::endl;
        // Fallback to empty if network unavailable, but test continues
    }

    // 2. Execute TraceOp
    Selector trace_addr = Selector{"jot/trace", {
        {"image", img_addr.to_json()},
        {"colors", 4},
        {"smooth", 1.0}
    }}.with_output("$out");

    std::cout << "  - Executing TraceOp (HSV Quantization)..." << std::endl;
    try {
        Processor::execute(&vfs, trace_addr);
    } catch (const std::exception& e) {
        std::cerr << "❌ TraceOp failed: " << e.what() << std::endl;
        return 1;
    }

    // 3. Verify results
    try {
        Shape result = vfs.read<Shape>(trace_addr);
        std::cout << "  - Number of color buckets: " << result.components.size() << std::endl;
        
        // 4. Render verification PNG
        Selector png_addr = Selector{"jot/png", {
            {"$in", trace_addr.to_json()},
            {"ax", 0.0}, 
            {"ay", 0.0}
        }}.with_output("$out");

        std::cout << "  - Generating verification PNG (actual/trace_test_final.png)..." << std::endl;
        Processor::execute(&vfs, png_addr);
        
        std::vector<uint8_t> png_bytes = vfs->read<std::vector<uint8_t>>(png_addr.with_output("file"));
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/trace_test_final.png", std::ios::binary);
        out.write((char*)png_bytes.data(), png_bytes.size());
        out.close();
        
        std::cout << "✅ Quantized Trace Test Complete." << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ Verification failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
