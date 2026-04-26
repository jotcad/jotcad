#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("on");
    register_all_ops(&vfs);
    
    std::cout << "Testing On (Conjugation) Operation..." << std::endl;
    
    fs::Selector hex_addr = fs::Selector{"jot/Hexagon/full", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_addr);
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    // 1. Get the corners of the box
    fs::Selector corners_sel = fs::Selector{"jot/corners", {{"$in", box_addr}}}.with_output("$out");
    Processor::execute(&vfs, corners_sel);

    // 2. On: [Hex] on [Box Corners] using [Rotate 45]
    // The 'op' template MUST explicitly target its output port ($out)
    fs::Selector op_template = fs::Selector{"jot/rotate", {{"angle", 45.0}}}.with_output("$out");
    
    fs::Selector on_addr = fs::Selector{"jot/on", {
        {"$in", hex_addr}, 
        {"target", corners_sel}, 
        {"op", op_template}
    }}.with_output("$out");
    
    Processor::execute(&vfs, on_addr);
    
    Shape s = vfs.read<Shape>(on_addr);
    if (s.components.empty()) {
        std::cerr << "❌ On FAIL: No components produced" << std::endl;
        return 1;
    }

    std::cout << "✅ On PASS" << std::endl;
    return 0;
}
