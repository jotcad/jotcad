#include "test_base.h"
#include "../ops/triangle_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("triangle_height_test");
    triangle_init(&vfs);

    std::cout << "Testing Triangle(height=100)..." << std::endl;

    fs::Selector sel("jot/Triangle/height");
    sel.parameters["height"] = 100.0;
    sel.output = "$out";

    Processor::execute(&vfs, sel);

    Shape out = vfs.read<Shape>(sel);
    Geometry geo = vfs.read<Geometry>(out.geometry.value());

    // height = 100
    // s = 100 / (sqrt(3)/2) = 200 / sqrt(3) approx 115.47
    // vertices: {-s/2, 0, 0}, {s/2, 0, 0}, {0, 100, 0}
    
    auto bbox = geo.bounds();
    double h = CGAL::to_double(bbox.ymax() - bbox.ymin());
    double w = CGAL::to_double(bbox.xmax() - bbox.xmin());

    std::cout << "  - Height: " << h << std::endl;
    std::cout << "  - Width: " << w << std::endl;

    assert(std::abs(h - 100.0) < 0.001);
    assert(std::abs(w - (200.0 / std::sqrt(3.0))) < 0.01);

    std::cout << "✨ Triangle height test passed!" << std::endl;
    return 0;
}
