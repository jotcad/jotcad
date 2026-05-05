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
    
    // 1. Verify $out is the shape
    Shape out = vfs.read<Shape>(pdf_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ PDF FAIL: No input shape returned" << std::endl;
        return 1;
    }

    // 2. Verify 'file' port has PDF data
    std::vector<uint8_t> pdf = vfs.read<std::vector<uint8_t>>(pdf_sel.with_output("file"));
    if (pdf.size() < 10 || pdf[0] != '%' || pdf[1] != 'P' || pdf[2] != 'D' || pdf[3] != 'F') {
        std::cerr << "❌ PDF FAIL: 'file' port missing or invalid PDF header (" << pdf.size() << " bytes)" << std::endl;
        return 1;
    }

    std::cout << "✅ PDF PASS" << std::endl;
    return 0;
}
