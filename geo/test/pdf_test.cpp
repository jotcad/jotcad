#include <iostream>
#include "../hexagon_op.h"
#include "../pdf_op.h"
#include "../../fs/cpp/include/vfs_node.h"

using namespace jotcad::geo;

int main() {
    fs::VFSNode::Config config = {"test-node", "1.0.0", ".vfs_storage_pdf_test"};
    fs::VFSNode vfs(config);
    
    std::cout << "Testing PDF Export..." << std::endl;
    
    fs::Selector hex_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    HexagonFullOp<>::execute(&vfs, hex_sel, 30.0);
    Shape hex = vfs.read<Shape>(hex_sel);
    
    fs::Selector pdf_sel = {"jot/pdf", {{"path", "test.pdf"}}};
    PdfOp<>::execute(&vfs, pdf_sel, hex, "test.pdf");
    
    // 1. Verify $out is the shape
    Shape out = vfs.read<Shape>(pdf_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ PDF FAIL: No input shape returned" << std::endl;
        return 1;
    }

    // 2. Verify 'file' port has PDF data
    std::vector<uint8_t> pdf = vfs.read<std::vector<uint8_t>>(pdf_sel, "file");
    if (pdf.size() < 10 || pdf[0] != '%' || pdf[1] != 'P' || pdf[2] != 'D' || pdf[3] != 'F') {
        std::cerr << "❌ PDF FAIL: 'file' port missing or invalid PDF header (" << pdf.size() << " bytes)" << std::endl;
        return 1;
    }

    std::cout << "✅ PDF PASS" << std::endl;
    return 0;
}
