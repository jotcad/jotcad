#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("on");
    register_all_ops(&vfs);
    
    std::cout << "Testing On (Conjugation) Operation..." << std::endl;
    
    fs::Selector hex_sel = {"jot/Hexagon/full", {{"diameter", 30.0}}};
    Processor::execute(&vfs, hex_sel);
    Shape hex = vfs.read<Shape>(hex_sel);
    
    fs::Selector corners_sel = {"jot/corners", {{"$in", hex_sel}, {"proxy", false}}};
    Processor::execute(&vfs, corners_sel);
    Shape corners = vfs.read<Shape>(corners_sel);
    
    // Operation to apply: offset(2) with explicit $in
    fs::Selector recipe = {"jot/offset", {{"$in", "$in"}, {"diameter", 2.0}}};
    
    // SCHEMA: {"$in", "target", "op"}
    fs::Selector on_sel = {"jot/on", {{"$in", hex_sel}, {"target", corners_sel}, {"op", recipe}}};
    Processor::execute(&vfs, on_sel);
    
    Shape out = vfs.read<Shape>(on_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ On FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ On PASS" << std::endl;
    return 0;
}
