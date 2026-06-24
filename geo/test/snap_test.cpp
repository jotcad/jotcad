#include "test_base.h"
#include <cassert>
#include <cmath>

using namespace jotcad::geo;

int main() {
    MockVFS vfs("snap");
    register_all_ops(&vfs);

    std::cout << "Testing Snap Operator & Directional Anchors..." << std::endl;

    // 1. Setup Box 'a' (10, 10, 10) and Box 'b' (5, 5, 5)
    fs::Selector a_sel = fs::Selector{"jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}}.with_output("$out");
    Processor::execute(&vfs, a_sel);
    Shape a = vfs.read<Shape>(a_sel);

    fs::Selector b_sel = fs::Selector{"jot/Box", {{"width", 5.0}, {"height", 5.0}, {"depth", 5.0}}}.with_output("$out");
    Processor::execute(&vfs, b_sel);
    Shape b = vfs.read<Shape>(b_sel);

    // 2. Tag 'b' as a gap
    fs::Selector b_gap_sel = fs::Selector{"jot/gap", {{"$in", vfs.materialize(b).value}}}.with_output("$out");
    Processor::execute(&vfs, b_gap_sel);
    Shape b_gap = vfs.read<Shape>(b_gap_sel);

    // 3. Evaluate bottom anchors of both 'a' and 'b_gap'
    fs::Selector a_bottom_sel = fs::Selector{"jot/bottom", {{"$in", vfs.materialize(a).value}}}.with_output("$out");
    Processor::execute(&vfs, a_bottom_sel);
    Shape a_bottom = vfs.read<Shape>(a_bottom_sel);

    fs::Selector b_bottom_sel = fs::Selector{"jot/bottom", {{"$in", vfs.materialize(b_gap).value}}}.with_output("$out");
    Processor::execute(&vfs, b_bottom_sel);
    Shape b_bottom = vfs.read<Shape>(b_bottom_sel);

    // Let's assert that the bottom anchor of Box(10,10,10) centered at 0 has z=-5
    double a_bot_z = CGAL::to_double(a_bottom.tf.t.cartesian(2, 3));
    std::cout << "  - Box 'a' bottom Z: " << a_bot_z << " (expected -5.0)" << std::endl;
    assert(std::abs(a_bot_z - (-5.0)) < 1e-6);

    // Let's assert that the bottom anchor of Box(5,5,5) centered at 0 has z=-2.5
    double b_bot_z = CGAL::to_double(b_bottom.tf.t.cartesian(2, 3));
    std::cout << "  - Box 'b' bottom Z: " << b_bot_z << " (expected -2.5)" << std::endl;
    assert(std::abs(b_bot_z - (-2.5)) < 1e-6);

    // 4. Perform snap: a.snap(bottom(), bottom(), b_gap)
    // Here:
    // source_anchor is a_bottom (jot/bottom evaluated on 'a')
    // target_anchor is bottom recipe (jot/bottom)
    // target_shape is b_gap
    fs::Selector snap_sel = fs::Selector{"jot/snap", {
        {"$in", vfs.materialize(a).value},
        {"source_anchor", vfs.materialize(a_bottom).value},
        {"target_anchor", fs::Selector{"jot/bottom"}},
        {"target_shape", vfs.materialize(b_gap).value}
    }}.with_output("$out");

    Processor::execute(&vfs, snap_sel);
    Shape snap_group = vfs.read<Shape>(snap_sel);

    // Assert that the snap operator returned a group containing two components:
    // [0]: snapped 'a'
    // [1]: target shape 'b_gap'
    assert(snap_group.components.size() == 2);
    std::cout << "  - Snap operation returned a group of 2 components" << std::endl;

    Shape snapped_a = snap_group.components[0];
    Shape snapped_b = snap_group.components[1];

    // Assert alignment: snapped_a's bottom should align with snapped_b's bottom
    fs::Selector snapped_a_bottom_sel = fs::Selector{"jot/bottom", {{"$in", vfs.materialize(snapped_a).value}}}.with_output("$out");
    Processor::execute(&vfs, snapped_a_bottom_sel);
    Shape snapped_a_bottom = vfs.read<Shape>(snapped_a_bottom_sel);

    double snapped_a_bot_z = CGAL::to_double(snapped_a_bottom.tf.t.cartesian(2, 3));
    std::cout << "  - Snapped Box 'a' bottom Z: " << snapped_a_bot_z << " (expected matches 'b' bottom: " << b_bot_z << ")" << std::endl;
    assert(std::abs(snapped_a_bot_z - b_bot_z) < 1e-6);

    // 5. Run disjoint to cut 'b_gap' out of 'snapped_a'
    fs::Selector disjoint_sel = fs::Selector{"jot/disjoint", {{"$in", vfs.materialize(snap_group).value}}}.with_output("$out");
    Processor::execute(&vfs, disjoint_sel);
    Shape disjoint_res = vfs.read<Shape>(disjoint_sel);

    // Disjoint results components
    Shape cut_a = disjoint_res.components[0];
    Geometry cut_a_geo = vfs.read<Geometry>(cut_a.geometry.value());

    // Compute volume of cut_a to verify that Box 'b' (125 volume) was subtracted from Box 'a' (1000 volume)
    // Box 'b' is completely inside 'a' because they both share their bottom face at Z=-2.5
    // and 'a' has height 10 (extends up to +7.5) while 'b' has height 5 (extends up to +2.5).
    // Fused volume should be 1000 - 125 = 875.
    double vol = vfs.read<double>(fs::Selector{"jot/volume", {{"$in", vfs.materialize(cut_a).value}}}.with_output("$out"));
    std::cout << "  - Disjoint cut_a volume: " << vol << " (expected 875.0)" << std::endl;
    assert(std::abs(vol - 875.0) < 1e-6);

    std::cout << "✅ snap_test Passed Successfully!" << std::endl;
    return 0;
}
