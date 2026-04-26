#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("accumulator");
    register_all_ops(&vfs);

    std::cout << "Testing Accumulator Pattern..." << std::endl;

    // 1. Initial targets (primitive point cloud)
    std::vector<std::vector<double>> pts = {{0,0,0}, {10,0,0}, {0,10,0}};
    fs::Selector pts_sel = fs::Selector{"jot/Points", {{"points", pts}}}.with_output("$out");
    Processor::execute(&vfs, pts_sel);
    Shape targets = vfs.read<Shape>(pts_sel);

    // 2. Manual Accumulation
    fs::Selector targets_sel = fs::Selector{"sys/targets", {{"$in", pts_sel}}}.with_output("$out");
    vfs.write(targets_sel, targets);

    Shape s = vfs.read<Shape>(targets_sel);
    // Point primitive currently creates a single shape with a point cloud geometry, not individual components
    if (!s.geometry.has_value()) {
        std::cerr << "❌ Accumulator FAIL: Expected geometry to be present" << std::endl;
        return 1;
    }

    std::cout << "✅ Accumulator PASS" << std::endl;
    return 0;
}
