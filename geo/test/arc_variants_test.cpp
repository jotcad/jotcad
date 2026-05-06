#include "test_base.h"
#include "arc_op.h"

using namespace jotcad::geo;
using namespace fs;

int main() {
    MockVFS vfs("arc_variants_test");
    register_all_ops(&vfs);

    std::cout << "Testing Arc Operation Variants..." << std::endl;

    // 1. Bounds-based Arc (Legacy fallback)
    {
        Selector sel = Selector{"jot/Arc", {{"diameter", 10.0}, {"start", 0.0}, {"end", 0.25}}}.with_output("$out");
        ArcBoundsOp<>::execute(&vfs, sel, Interval{-5.0, 5.0}, Interval{-5.0, 5.0}, Interval{-5.0, 5.0}, 0.0, 0.25, 0.1);
        Shape s = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(s.geometry.value());
        std::cout << "  - Bounds Arc (90 deg) segments: " << geo.segments.size() << std::endl;
        assert(!geo.segments.empty());
        // Check if it's open (segments = vertices - 1)
        assert(geo.segments.size() == geo.vertices.size() - 1);
    }

    // 2. 2-Point + Radius Arc
    {
        Shape p1; // Origin
        Shape p2; p2.tf = Matrix::translate(10, 0, 0); // (10, 0, 0)
        
        Selector sel = Selector{"jot/Arc/2p", {{"radius", 10.0}}}.with_output("$out");
        Arc2POp<>::execute(&vfs, sel, p1, p2, 10.0, false, false, 0.1);
        
        Shape s = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(s.geometry.value());
        std::cout << "  - 2P Arc (r=10) segments: " << geo.segments.size() << std::endl;
        std::cout << "    - Start vertex: (" << geo.vertices.front().x << ", " << geo.vertices.front().y << ")" << std::endl;
        assert(!geo.segments.empty());
        
        // Midpoint check: Arc from (0,0) to (10,0) with r=10 (short arc, CCW)
        // Center should be at (5, -sqrt(100 - 25)) = (5, -8.66) ? No, SVG logic...
        // Let's just check vertex count and basic connectivity
        assert(geo.vertices.front().x == 0);
        assert(geo.vertices.back().x == 10);
    }

    // 3. 3-Point Arc
    {
        Shape p1; // (0,0,0)
        Shape p2; p2.tf = Matrix::translate(5, 5, 0); // Midpoint
        Shape p3; p3.tf = Matrix::translate(10, 0, 0); // End
        
        Selector sel = Selector{"jot/Arc/3p", {}}.with_output("$out");
        Arc3POp<>::execute(&vfs, sel, p1, p2, p3, 0.1);
        
        Shape s = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(s.geometry.value());
        std::cout << "  - 3P Arc (0,0 -> 5,5 -> 10,0) segments: " << geo.segments.size() << std::endl;
        assert(!geo.segments.empty());
        
        // This should be a semi-circle of diameter 10, radius 5, center (5,0)
        assert(geo.vertices.front().x == 0);
        assert(geo.vertices.back().x == 10);
        // Midpoint should be near (5,5)
        bool found_mid = false;
        for(auto& v : geo.vertices) {
            if (std::abs(CGAL::to_double(v.x) - 5.0) < 0.1 && std::abs(CGAL::to_double(v.y) - 5.0) < 0.1) {
                found_mid = true;
                break;
            }
        }
        assert(found_mid);
    }

    std::cout << "✅ Arc Variants PASS" << std::endl;
    return 0;
}
