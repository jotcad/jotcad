#include "test_base.h"
#include "../ops/offset_op.h"
#include "../ops/box_op.h"
#include "../ops/group_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("offset_recursive_test");
    offset_init(&vfs);
    box_init(&vfs);
    group_init(&vfs);

    std::cout << "Testing Recursive Offset on Group..." << std::endl;

    // 1. Create two 10x10 boxes
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.output = "$out";

    Processor::execute(&vfs, box_sel);
    Shape box = vfs.read<Shape>(box_sel);

    // 2. Put them in a group
    Shape group;
    group.add_tag("type", "group");
    group.components.push_back(box);
    group.components.push_back(box);

    // 3. Apply Offset(diameter=5) to the group
    fs::Selector offset_sel("jot/offset");
    offset_sel.parameters["$in"] = group;
    offset_sel.parameters["diameter"] = 5.0;
    offset_sel.output = "$out";

    // This should NOT throw even though 'group' has no geometry
    Processor::execute(&vfs, offset_sel);

    Shape out = vfs.read<Shape>(offset_sel);
    assert(out.components.size() == 2);

    // 4. Verify each child was offset
    for (const auto& child : out.components) {
        assert(child.geometry.has_value());
        Geometry geo = vfs.read<Geometry>(child.geometry.value());
        auto bbox = geo.bounds();
        
        // Original 10x10 box: [-5, 5] x [-5, 5]
        // Offset 5: [-10, 10] x [-10, 10] (Size 20x20)
        double w = CGAL::to_double(bbox.xmax() - bbox.xmin());
        double h = CGAL::to_double(bbox.ymax() - bbox.ymin());
        
        std::cout << "  - Child Size: " << w << "x" << h << std::endl;
        assert(std::abs(w - 20.0) < 0.1);
        assert(std::abs(h - 20.0) < 0.1);
    }

    std::cout << "✨ Recursive offset test passed!" << std::endl;
    return 0;
}
