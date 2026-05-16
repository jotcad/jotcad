#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("on_at");
    register_all_ops(&vfs);
    
    std::cout << "Testing At (Anchor Pattern) & On (Targeted Replacement)..." << std::endl;
    
    // Setup: A Hexagon and a Box
    fs::Selector hex_addr = fs::Selector{"jot/Hexagon/diameter", {{"diameter", 30.0}}}.with_output("$out");
    Processor::execute(&vfs, hex_addr);
    
    fs::Selector box_addr = fs::Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, box_addr);
    
    // Get the corners of the box
    fs::Selector corners_sel = fs::Selector{"jot/corners", {{"$in", box_addr}}}.with_output("$out");
    Processor::execute(&vfs, corners_sel);

    // 1. Test 'at': [Hex] at [Box Corners] using [Rotate 45]
    // Refined semantics: 'at' applies op sequentially to the subject.
    fs::Selector at_addr = fs::Selector{"jot/at", {
        {"$in", hex_addr}, 
        {"target", corners_sel}, 
        {"op", fs::Selector{"jot/rotate", {{"angle", 45.0}}}.with_output("$out")}
    }}.with_output("$out");
    
    Processor::execute(&vfs, at_addr);
    Shape s_at = vfs.read<Shape>(at_addr);
    
    if (!s_at.geometry.has_value()) {
        std::cerr << "❌ At FAIL: No geometry produced" << std::endl;
        return 1;
    }
    std::cout << "  ✅ At (Anchor Pattern) PASS" << std::endl;

    // 2. Test 'on': Targeted Replacement
    // We create a Group of [Hex] and try to replace the Hex within it using 'on'.
    Shape assembly;
    assembly.components.push_back(vfs.read<Shape>(hex_addr));
    fs::CID assembly_cid = vfs.materialize(assembly);

    fs::Selector on_addr = fs::Selector{"jot/on", {
        {"$in", assembly_cid.value}, 
        {"target", hex_addr.to_json()}, 
        {"op", fs::Selector{"jot/rotate", {{"angle", 90.0}}}.with_output("$out").to_json()}
    }}.with_output("$out");

    Processor::execute(&vfs, on_addr);
    Shape s_on = vfs.read<Shape>(on_addr);

    if (s_on.components.empty() || !s_on.components[0].geometry.has_value()) {
        std::cerr << "❌ On FAIL: Target not replaced in hierarchy" << std::endl;
        return 1;
    }
    std::cout << "  ✅ On (Targeted Replacement) PASS" << std::endl;

    return 0;
}
