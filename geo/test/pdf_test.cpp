#include "test_base.h"
#include "../hexagon_op.h"
#include "../pdf_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing PDF Passthrough..." << std::endl;
    MockVFS vfs;
    
    Shape hex;
    HexagonOp<hex_full>::execute(&vfs, {30.0}, hex);
    
    Shape out;
    PdfOp<>::execute(&vfs, hex, "test.pdf", 0.1, out);
    
    assert(out.geometry.path == hex.geometry.path);
    
    std::cout << "✅ PDF PASS" << std::endl;
    return 0;
}
