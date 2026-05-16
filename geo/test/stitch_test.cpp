#include "test_base.h"
#include "box_op.h"
#include "outline_op.h"
#include "stitch_op.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("stitch_test");
    register_all_ops(&vfs);

    // 1. Create a 100mm line (path)
    // We'll use Box(100, 0, 0) which creates a segment
    fs::Selector line_sel = fs::Selector("jot/Box", {{"width", 100.0}, {"height", 0.0}, {"depth", 0.0}}).with_output("$out");
    Shape line = vfs.read<Shape>(line_sel);
    Geometry line_geo = vfs.read<Geometry>(line.geometry.value());
    std::cout << "Input Line segments: " << line_geo.segments.size() << std::endl;

    // 2. Test Complex Pattern: 
    // start=[5] (5mm ON at start)
    // end=[-5] (5mm ON at end)
    // repeat=[-10, 2] (10mm ON, 2mm OFF, anchored to END)
    // Total L = 100.
    // Start: [0, 5]
    // End: [95, 100]
    // Gap: 5 to 95 (90mm total)
    // Repeat period: 12mm.
    // Anchored to 95:
    // 85-95 (ON)
    // 83-85 (OFF)
    // 71-83 (ON)
    // ...
    // 90 / 12 = 7 full periods (84mm) + 6mm remainder.
    // 6mm remainder is at the start of the gap (5mm to 11mm).
    // The repeat pattern starts from the END, so:
    // Stitch 1: 85-95 (10)
    // Stitch 2: 73-83 (10)
    // Stitch 3: 61-71 (10)
    // Stitch 4: 49-59 (10)
    // Stitch 5: 37-47 (10)
    // Stitch 6: 25-35 (10)
    // Stitch 7: 13-23 (10)
    // Stitch 8: 5-11 (partial 6mm ON)
    
    fs::Selector stitch_sel = fs::Selector("jot/stitch", {
        {"$in", line_sel},
        {"start", {5.0}},
        {"repeat", {-10.0, 2.0}},
        {"end", {-5.0}}
    }).with_output("$out");

    Shape stitched = vfs.read<Shape>(stitch_sel);
    Geometry stitched_geo = vfs.read<Geometry>(stitched.geometry.value());
    
    std::cout << "Stitched segments: " << stitched_geo.segments.size() << std::endl;

    // We expect:
    // 1 (start) + 8 (repeat) + 1 (end) = 10 segments
    if (stitched_geo.segments.size() != 10) {
        std::cerr << "FAIL: Expected 10 stitched segments, got " << stitched_geo.segments.size() << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: Complex stitch patterning verified." << std::endl;
    return 0;
}
