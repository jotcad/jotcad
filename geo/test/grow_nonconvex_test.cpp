#include "test_base.h"
#include "protocols.h"
#include "processor.h"

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("grow_nonconvex");
    register_all_ops(&vfs);

    std::cout << "Testing Grow with Non-Convex / Segmented Geometry..." << std::endl;

    // 1. L-shape wire (two segments: (0,10)->(0,0) and (0,0)->(10,0))
    Geometry l_wire_geo;
    l_wire_geo.vertices = {
        {FT(0), FT(10), FT(0)},
        {FT(0), FT(0), FT(0)},
        {FT(10), FT(0), FT(0)}
    };
    l_wire_geo.segments.push_back({0, 1});
    l_wire_geo.segments.push_back({1, 2});

    Shape l_wire;
    l_wire.geometry = vfs.materialize(l_wire_geo);
    l_wire.add_tag("type", "wire");

    // 2. Small square tool (2x2 centered at 0,0)
    fs::Selector tool_sel;
    tool_sel.path = "jot/Box";
    tool_sel.parameters["width"] = 2.0;
    tool_sel.parameters["height"] = 2.0;
    tool_sel.parameters["depth"] = 0.0;
    tool_sel = tool_sel.with_output("$out");
    Shape tool = vfs.read<Shape>(tool_sel);

    // 3. Grow
    fs::Selector grow_sel;
    grow_sel.path = "jot/grow";
    grow_sel.parameters["$in"] = l_wire.to_json();
    grow_sel.parameters["tool"] = tool.to_json();
    grow_sel = grow_sel.with_output("$out");

    std::cout << "  - Executing jot/grow (L-wire + Square)..." << std::endl;
    Shape result = vfs.read<Shape>(grow_sel);
    Geometry res_geo = vfs.read<Geometry>(result.geometry.value());

    // 4. Verify result.
    // If the segments were grown separately, the area around (5,5) should be empty.
    // We check a point at (5,5).
    
    bool point_inside = false;
    for (const auto& face : res_geo.faces) {
        if (face.loops.empty()) continue;
        boolean::Polygon_2 poly;
        for (int idx : face.loops[0]) {
            poly.push_back(EK::Point_2(res_geo.vertices[idx].x, res_geo.vertices[idx].y));
        }
        if (poly.bounded_side(EK::Point_2(5, 5)) == CGAL::ON_BOUNDED_SIDE) {
            point_inside = true;
            break;
        }
    }

    if (point_inside) {
        std::cerr << "    - FAIL: Point (5,5) is INSIDE the grown geometry (Global Hull detected)." << std::endl;
        return 1;
    } else {
        std::cout << "    - SUCCESS: Point (5,5) is OUTSIDE the grown geometry (Decomposed Growth detected)." << std::endl;
    }

    return 0;
}
