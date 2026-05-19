#include "test_base.h"
#include <string>
#include <algorithm>

using namespace jotcad::geo;

int main() {
    MockVFS vfs("pdf_linewidth");
    register_all_ops(&vfs);
    
    // REGRESSION TEST: The PDF line width must be exactly 0.072pt (hairline).
    // This is CRITICAL for laser cutting compatibility. DO NOT CHANGE.
    std::cout << "Testing PDF Line Width (0.072 w)..." << std::endl;
    
    fs::Selector hex_sel = fs::Selector{"jot/Hexagon/radius", {{"radius", 15.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_sel);
    
    fs::Selector pdf_sel = fs::Selector{"jot/pdf", {{"$in", hex_sel}, {"path", "test.pdf"}}}.with_output("$out");
    Processor::execute(&vfs, pdf_sel);
    
    std::vector<uint8_t> pdf = vfs.read<std::vector<uint8_t>>(pdf_sel);
    std::string pdf_str(pdf.begin(), pdf.end());
    
    if (pdf_str.find("0.072 w") == std::string::npos) {
        std::cerr << "❌ PDF LINEWIDTH FAIL: Could not find '0.072 w' in PDF stream" << std::endl;
        // Print a bit of the stream for context if it's there but different
        size_t q_pos = pdf_str.find("q\n");
        if (q_pos != std::string::npos) {
            std::cerr << "Found stream start, context: " << pdf_str.substr(q_pos, 20) << "..." << std::endl;
        }
        return 1;
    }

    std::cout << "✅ PDF LINEWIDTH PASS" << std::endl;
    return 0;
}
