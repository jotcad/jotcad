#include "test_base.h"
#include <cassert>

using namespace jotcad::geo;

int main() {
    MockVFS vfs("spin");
    register_all_ops(&vfs);
    
    std::cout << "Testing Spin Operation with zag..." << std::endl;
    
    // 1. Create a Point shape at (10, 0, 0)
    fs::Selector pt_fulfilling = fs::Selector{"jot/Point", {{"x", 10.0}, {"y", 0.0}, {"z", 0.0}}}.with_output("$out");
    Processor::execute(&vfs, pt_fulfilling);
    Shape pt_shape = vfs.read<Shape>(pt_fulfilling);
    
    // Verify point geometry is valid
    assert(pt_shape.geometry.has_value());
    Geometry pt_geo = vfs.read<Geometry>(pt_shape.geometry.value());
    assert(pt_geo.vertices.size() == 1);
    
    // 2. Perform spin with zag = 0.5, start = 0.0, end = 1.0 (full circle)
    // Radius R = 10, Diameter D = 20, zag = 0.5.
    // k = 0.5 / 10 = 0.05
    // sides = ceil(pi / sqrt(2 * k)) = ceil(pi / sqrt(0.1)) = ceil(3.14159 / 0.3162277) = ceil(9.934) = 10.
    // Expected resolution = 10. Since it is full circle, num_slices = 10.
    // Total vertices should be 10 * 1 = 10.
    fs::Selector spin_full = fs::Selector{"jot/spin", {
        {"$in", pt_fulfilling},
        {"start", 0.0},
        {"end", 1.0},
        {"resolution", 32}, // overridden by zag
        {"zag", 0.5}
    }}.with_output("$out");
    Processor::execute(&vfs, spin_full);
    
    Shape spin_full_shape = vfs.read<Shape>(spin_full);
    assert(spin_full_shape.geometry.has_value());
    Geometry spin_full_geo = vfs.read<Geometry>(spin_full_shape.geometry.value());
    std::cout << "Full spin vertices count: " << spin_full_geo.vertices.size() << " (Expected: 10)" << std::endl;
    assert(spin_full_geo.vertices.size() == 10);
    
    // 3. Perform partial spin with zag = 0.5, start = 0.0, end = 0.25 (quarter turn)
    // total_tau = 0.25.
    // resolution = ceil(0.25 * 10) = 3.
    // Since it is partial circle, num_slices = resolution + 1 = 4.
    // Total vertices should be 4 * 1 = 4.
    fs::Selector spin_part = fs::Selector{"jot/spin", {
        {"$in", pt_fulfilling},
        {"start", 0.0},
        {"end", 0.25},
        {"resolution", 32}, // overridden by zag
        {"zag", 0.5}
    }}.with_output("$out");
    Processor::execute(&vfs, spin_part);
    
    Shape spin_part_shape = vfs.read<Shape>(spin_part);
    assert(spin_part_shape.geometry.has_value());
    Geometry spin_part_geo = vfs.read<Geometry>(spin_part_shape.geometry.value());
    std::cout << "Partial spin vertices count: " << spin_part_geo.vertices.size() << " (Expected: 4)" << std::endl;
    assert(spin_part_geo.vertices.size() == 4);

    std::cout << "✅ Spin with zag PASS" << std::endl;
    return 0;
}
