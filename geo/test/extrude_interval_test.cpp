#include "ops/extrude_op.h"
#include "ops/box_op.h"
#include "test_base.h"

using namespace jotcad::geo;

// Local bounds helper
struct Bounds {
    double min_x = 1e18, max_x = -1e18;
    double min_y = 1e18, max_y = -1e18;
    double min_z = 1e18, max_z = -1e18;
};

Bounds get_geo_bounds(const Geometry& geo) {
    Bounds b;
    for (const auto& v : geo.vertices) {
        double x = CGAL::to_double(v.x);
        double y = CGAL::to_double(v.y);
        double z = CGAL::to_double(v.z);
        if (x < b.min_x) b.min_x = x;
        if (x > b.max_x) b.max_x = x;
        if (y < b.min_y) b.min_y = y;
        if (y > b.max_y) b.max_y = y;
        if (z < b.min_z) b.min_z = z;
        if (z > b.max_z) b.max_z = z;
    }
    return b;
}

Shape create_box(fs::VFSNode* vfs, double w, double h, double d) {
    fs::Selector sel("jot/Box");
    sel.output = "$out";
    BoxOp<>::execute(vfs, sel, Interval{0.0, w}, Interval{0.0, h}, Interval{0.0, d});
    return vfs->read<Shape>(sel);
}

void test_extrude_z_interval() {
    MockVFS vfs("extrude_z_interval");
    register_all_ops(&vfs);

    Shape s = create_box(&vfs, 10, 10, 0); // Flat box
    
    // Extrude from Z=10 to Z=25
    fs::Selector sel("jot/extrudeZ");
    sel.parameters["$in"] = s;
    sel.parameters["height"] = {10.0, 25.0};
    sel.output = "$out";
    
    Shape res = vfs.read<Shape>(sel);
    
    assert(res.geometry.has_value());
    Geometry geo = vfs.read<Geometry>(res.geometry.value());
    
    Bounds bounds = get_geo_bounds(geo);
    assert(std::abs(bounds.min_z - 10.0) < 1e-6);
    assert(std::abs(bounds.max_z - 25.0) < 1e-6);
    std::cout << "  ✅ ExtrudeZInterval PASS" << std::endl;
}

void test_extrude_to_reference() {
    MockVFS vfs("extrude_to_ref");
    register_all_ops(&vfs);

    Shape s = create_box(&vfs, 10, 10, 0); // Flat box at origin
    
    // Reference frame at Z=50, rotated 90deg around X (0.25 turns)
    Shape ref;
    ref.tf = Matrix::translate(0, 0, 50) * Matrix::rotationX(0.25); 
    
    // Extrude to reference frame with range [0, 5]
    fs::Selector sel("jot/extrude");
    sel.parameters["$in"] = s;
    sel.parameters["target"] = ref;
    sel.parameters["range"] = {0.0, 5.0};
    sel.output = "$out";
    
    Shape res = vfs.read<Shape>(sel);
    
    assert(res.geometry.has_value());
    Geometry geo = vfs.read<Geometry>(res.geometry.value());
    
    // Transform back to ref space to check local bounds
    Matrix to_ref = ref.tf.inverse();
    Geometry local_geo = geo;
    local_geo.apply_tf(to_ref);
    
    Bounds bounds = get_geo_bounds(local_geo);
    assert(std::abs(bounds.min_z - 0.0) < 1e-6);
    assert(std::abs(bounds.max_z - 5.0) < 1e-6);
    std::cout << "  ✅ ExtrudeToReferenceFrame PASS" << std::endl;
}

int main() {
    std::cout << "Testing Extrude Interval & Reference..." << std::endl;
    test_extrude_z_interval();
    test_extrude_to_reference();
    std::cout << "✅ ALL Extrude Tests Passed" << std::endl;
    return 0;
}
