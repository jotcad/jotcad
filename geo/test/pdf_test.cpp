#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("pdf");
    register_all_ops(&vfs);
    
    std::cout << "Testing PDF Export..." << std::endl;
    
    fs::Selector hex_sel = fs::Selector{"jot/Hexagon/radius", {{"radius", 15.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_sel);
    
    fs::Selector pdf_sel = fs::Selector{"jot/pdf", {{"$in", hex_sel}, {"path", "test.pdf"}}}.with_output("$out");
    Processor::execute(&vfs, pdf_sel);
    
    // 1. Verify $out has PDF data
    std::vector<uint8_t> pdf = vfs.read<std::vector<uint8_t>>(pdf_sel);
    if (pdf.size() < 10 || pdf[0] != '%' || pdf[1] != 'P' || pdf[2] != 'D' || pdf[3] != 'F') {
        std::cerr << "❌ PDF FAIL: '$out' port missing or invalid PDF header (" << pdf.size() << " bytes)" << std::endl;
        return 1;
    }

    std::cout << "✅ PDF PASS" << std::endl;
    return 0;
}
