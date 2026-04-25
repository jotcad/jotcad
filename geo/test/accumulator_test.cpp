#include "test_base.h"
#include "impl/matrix.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("accumulator");
    register_all_ops(&vfs);
    
    std::cout << "Testing Accumulator (On) Operation..." << std::endl;
    
    fs::Selector base_sel = {"jot/Box", {{"width", 50.0}, {"height", 50.0}, {"depth", 0.0}, {"id", "base"}}};
    Processor::execute(&vfs, base_sel);
    Shape base = vfs.read<Shape>(base_sel);

    // Create target points
    Shape targets;
    targets.components.resize(3);
    targets.components[0].tf = Matrix::translate(10, 0, 0).to_vec();
    targets.components[0].geometry = base.geometry;
    targets.components[1].tf = Matrix::translate(20, 0, 0).to_vec();
    targets.components[1].geometry = base.geometry;
    targets.components[2].tf = Matrix::translate(30, 0, 0).to_vec();
    targets.components[2].geometry = base.geometry;
    
    fs::Selector targets_sel("test/targets", json::object(), "$out");
    vfs.write<Shape>(targets_sel, targets);

    // Operation: offset(2) with explicit $in placeholder
    fs::Selector recipe = {"jot/offset", {{"$in", "$in"}, {"diameter", 2.0}}};
    
    // SCHEMA: {"$in", "target", "op"}
    fs::Selector on_sel = {"jot/on", {{"$in", base_sel}, {"target", targets_sel}, {"op", recipe}}};
    Processor::execute(&vfs, on_sel);
    
    Shape out = vfs.read<Shape>(on_sel);
    if (!out.geometry.has_value()) {
        std::cerr << "❌ Accumulator FAIL: No result geometry" << std::endl;
        return 1;
    }

    std::cout << "✅ Accumulator PASS" << std::endl;
    return 0;
}
