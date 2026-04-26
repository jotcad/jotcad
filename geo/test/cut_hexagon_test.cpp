#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut_hex");
    register_all_ops(&vfs);
    
    std::cout << "Testing Cut Hexagon Operation..." << std::endl;
    
    fs::Selector base_sel = fs::Selector{"jot/Hexagon/full", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, base_sel);

    fs::Selector tool_sel = fs::Selector{"jot/Triangle/equilateral", {{"side", 2.0}}}.with_output("$out");
    Processor::execute(&vfs, tool_sel);
    
    fs::Selector cut_sel = fs::Selector{"jot/cut", {{"$in", base_sel}, {"tools", {tool_sel}}}}.with_output("$out");
    Processor::execute(&vfs, cut_sel);
    
    Shape out = vfs.read<Shape>(cut_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Cut Hexagon FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Cut Hexagon PASS" << std::endl;
    return 0;
}
