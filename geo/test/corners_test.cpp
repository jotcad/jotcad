#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("corners");
    register_all_ops(&vfs);
    
    std::cout << "Testing Corners Operation..." << std::endl;
    
    fs::Selector hex_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    Processor::execute(&vfs, hex_sel);
    Shape hex = vfs.read<Shape>(hex_sel);
    
    fs::Selector corners_sel = {"jot/corners", {{"$in", hex_sel}, {"proxy", true}}};
    Processor::execute(&vfs, corners_sel);
    
    Shape out = vfs.read<Shape>(corners_sel);
    if (out.components.size() != 6) {
        std::cerr << "❌ Corners FAIL: Expected 6 corner components, got " << out.components.size() << std::endl;
        return 1;
    }

    std::cout << "✅ Corners PASS" << std::endl;
    return 0;
}
