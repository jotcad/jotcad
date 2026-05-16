#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("offset");
    register_all_ops(&vfs);
    
    std::cout << "Testing Offset Operation..." << std::endl;
    
    fs::Selector hex_fulfilling = fs::Selector{"jot/Hexagon/diameter", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_fulfilling);
    
    fs::Selector offset_fulfilling = fs::Selector{"jot/offset", {{"$in", hex_fulfilling}, {"diameter", 5.0}}}.with_output("$out");
    Processor::execute(&vfs, offset_fulfilling);
    
    Shape s = vfs.read<Shape>(offset_fulfilling);
    vfs.verify_render(s, "offset_op_basic", "3f702aa7105058b23a9eaf57fea8372d8f4b5fb8290f458b2785b90d740f472c");

    std::cout << "✅ Offset PASS" << std::endl;
    return 0;
}
