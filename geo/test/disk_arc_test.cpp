#include "test_base.h"
#include "disk_op.h"
#include "arc_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("disk_arc_test");
    register_all_ops(&vfs);

    std::cout << "Testing Disk and Arc Operations..." << std::endl;

    // 1. Full Disk (Circle)
    Selector disk_sel = Selector{"jot/Disk", {{"diameter", 10.0}}}.with_output("$out");
    DiskOp<>::execute(&vfs, disk_sel, Interval{-5.0, 5.0}, Interval{0, 0}, Interval{0, 0}, 0.0, 1.0, 0.1);
    Shape disk = vfs.read<Shape>(disk_sel);
    Geometry d_geo = vfs.read<Geometry>(disk.geometry.value());
    std::cout << "  - Circle Disk (diameter 10) vertices: " << d_geo.vertices.size() << std::endl;
    assert(!d_geo.faces.empty());
    assert(d_geo.vertices.size() > 8);

    // 2. Full Arc (Circle boundary)
    Selector arc_sel = Selector{"jot/Arc", {{"diameter", 10.0}}}.with_output("$out");
    ArcOp<>::execute(&vfs, arc_sel, Interval{-5.0, 5.0}, Interval{0, 0}, Interval{0, 0}, 0.0, 1.0, 0.1);
    Shape arc = vfs.read<Shape>(arc_sel);
    Geometry a_geo = vfs.read<Geometry>(arc.geometry.value());
    std::cout << "  - Circle Arc segments: " << a_geo.segments.size() << std::endl;
    assert(!a_geo.segments.empty());
    assert(a_geo.segments.size() == a_geo.vertices.size()); // Closed loop

    // 3. Ellipse Disk (20x10)
    Selector ell_sel = Selector{"jot/Disk", {{"width", 20.0}, {"height", 10.0}}}.with_output("$out");
    DiskOp<>::execute(&vfs, ell_sel, Interval{-5.0, 5.0}, Interval{-10.0, 10.0}, Interval{-5.0, 5.0}, 0.0, 1.0, 0.1);
    Shape ell = vfs.read<Shape>(ell_sel);
    Geometry e_geo = vfs.read<Geometry>(ell.geometry.value());
    std::cout << "  - Ellipse Disk (20x10) vertices: " << e_geo.vertices.size() << std::endl;
    assert(e_geo.vertices.size() > 8);

    std::cout << "✅ Disk and Arc PASS" << std::endl;
    return 0;
}
