#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("corners");
    register_all_ops(&vfs);
    
    std::cout << "Testing Corners Operation..." << std::endl;
    
    fs::Selector hex_sel = fs::Selector{"jot/Hexagon/diameter", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_sel);
    
    fs::Selector corners_sel = fs::Selector{"jot/corners", {{"$in", hex_sel}}}.with_output("$out");
    Processor::execute(&vfs, corners_sel);
    
    Shape out = vfs.read<Shape>(corners_sel);
    // Now it just returns the input shape with updated geometry (point cloud)
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Corners FAIL: Expected geometry to be present. Shape tags: " << out.tags.dump() << std::endl;
        return 1;
    } else {
        std::cout << "  - Output geometry present: " << out.geometry.value().value << std::endl;
    }

    std::cout << "✅ Corners PASS" << std::endl;
    return 0;
}
