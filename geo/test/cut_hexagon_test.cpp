#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut_hex");
    register_all_ops(&vfs);
    
    std::cout << "Testing Cut Hexagon Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    Processor::execute(&vfs, base_sel);
    Shape base = vfs.read<Shape>(base_sel);

    fs::Selector tool_sel = {"jot/Triangle/equilateral", {{"side", 2.0}}};
    Processor::execute(&vfs, tool_sel);
    Shape tool = vfs.read<Shape>(tool_sel);
    
    fs::Selector cut_sel = {"jot/cut", {{"$in", base_sel}, {"tools", {tool_sel}}}};
    Processor::execute(&vfs, cut_sel);
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Cut Hexagon FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Cut Hexagon PASS" << std::endl;
    return 0;
}
