#include "test_base.h"
#include "../ops/pack_op.h"

using namespace jotcad::geo;

Shape create_triangle(fs::VFSNode* vfs, double size) {
    Geometry geo;
    geo.vertices = {
        {FT(0), FT(0), FT(0)},
        {FT(size), FT(0), FT(0)},
        {FT(0), FT(size), FT(0)}
    };
    geo.faces.push_back({{{0, 1, 2}}});
    
    Shape s;
    s.geometry = vfs->materialize(geo);
    s.tags["type"] = "surface";
    return s;
}

Shape create_box(fs::VFSNode* vfs, double w, double h) {
    Geometry geo;
    geo.vertices = {
        {FT(0), FT(0), FT(0)},
        {FT(w), FT(0), FT(0)},
        {FT(w), FT(h), FT(0)},
        {FT(0), FT(h), FT(0)}
    };
    geo.faces.push_back({{{0, 1, 2, 3}}});
    
    Shape s;
    s.geometry = vfs->materialize(geo);
    s.tags["type"] = "surface";
    return s;
}

void test_multi_sheet() {
    MockVFS vfs("pack_test");
    pack_init(&vfs);

    std::cout << "Testing Multi-Sheet Packing..." << std::endl;

    Shape group;
    group.tags["type"] = "group";
    for (int i = 0; i < 5; ++i) {
        group.components.push_back(create_triangle(&vfs, 50));
    }

    Shape sheets_group;
    sheets_group.tags["type"] = "group";
    // 5 triangles of 50x50. 
    // In a 110x110 sheet, we can fit 4 (2x2) without rotation optimization.
    // 5 will need a second sheet.
    sheets_group.components.push_back(create_box(&vfs, 110, 110));
    sheets_group.components.push_back(create_box(&vfs, 110, 110));

    fs::Selector sel("jot/pack");
    sel.parameters["$in"] = group;
    sel.parameters["sheet"] = sheets_group;
    sel.parameters["spacing"] = 2.0;
    sel.output = "$out";

    Processor::execute(&vfs, sel);

    Shape out = vfs.read<Shape>(sel);
    assert(out.tags["type"] == "group");
    
    bool found_sheet_0 = false;
    bool found_sheet_1 = false;
    int total_placed_parts = 0;

    for (const auto& comp : out.components) {
        if (comp.tags["name"] == "sheet_0") {
            found_sheet_0 = true;
            total_placed_parts += (int)comp.components.size() - 1;
        } else if (comp.tags["name"] == "sheet_1") {
            found_sheet_1 = true;
            total_placed_parts += (int)comp.components.size() - 1;
        }
    }

    assert(found_sheet_0);
    assert(found_sheet_1);
    std::cout << "  - Total placed parts: " << total_placed_parts << " / 5" << std::endl;
}

void test_alignment_and_bias() {
    MockVFS vfs("pack_align");
    pack_init(&vfs);
    std::cout << "Testing Centered Sheet Alignment and Top-Left Bias..." << std::endl;

    // 1. Create a 10x10 Box (centered at 0,0)
    // Bounds: [-5, 5]
    Geometry box_geo;
    box_geo.vertices = {
        {FT(-5), FT(-5), FT(0)},
        {FT(5), FT(-5), FT(0)},
        {FT(5), FT(5), FT(0)},
        {FT(-5), FT(5), FT(0)}
    };
    box_geo.faces.push_back({{{0, 1, 2, 3}}});
    Shape part;
    part.geometry = vfs.materialize(box_geo);

    // 2. Create a 50x50 Sheet (centered at 0,0)
    // Bounds: [-25, 25]
    Geometry sheet_geo;
    sheet_geo.vertices = {
        {FT(-25), FT(-25), FT(0)},
        {FT(25), FT(-25), FT(0)},
        {FT(25), FT(25), FT(0)},
        {FT(-25), FT(25), FT(0)}
    };
    sheet_geo.faces.push_back({{{0, 1, 2, 3}}});
    Shape sheet;
    sheet.geometry = vfs.materialize(sheet_geo);

    // 3. Pack
    fs::Selector sel("jot/pack");
    sel.parameters["$in"] = part;
    sel.parameters["sheet"] = sheet;
    sel.parameters["spacing"] = 0.0;
    sel.parameters["margin"] = 0.0;
    sel.output = "$out";

    Processor::execute(&vfs, sel);

    // 4. Verify part is INSIDE sheet bounds [-25, 25]
    Shape out = vfs.read<Shape>(sel);
    Shape sheet_grp = out.components[0];
    Shape placed_part = sheet_grp.components[1];
    
    Geometry p_geo = vfs.read<Geometry>(placed_part.geometry.value());
    p_geo.apply_tf(placed_part.tf);
    
    auto bbox = p_geo.bounds();
    double xmin = CGAL::to_double(bbox.xmin());
    double xmax = CGAL::to_double(bbox.xmax());
    double ymin = CGAL::to_double(bbox.ymin());
    double ymax = CGAL::to_double(bbox.ymax());

    std::cout << "  - Placed part bounds: [" << xmin << ", " << xmax << "] x [" << ymin << ", " << ymax << "]" << std::endl;
    
    // Check inside bounds
    assert(xmin >= -25.0001);
    assert(xmax <= 25.0001);
    assert(ymin >= -25.0001);
    assert(ymax <= 25.0001);

    // Check Bottom-Left Bias
    // With spacing 0, it should be exactly at the bottom-left corner.
    // Sheet x is [-25, 25], Sheet y is [-25, 25].
    // Bottom-Left corner is (-25, -25).
    // Part size is 10x10.
    // So part should be at [-25, -15] x [-25, -15].
    
    std::cout << "  - Verifying Bottom-Left Bias..." << std::endl;
    assert(std::abs(xmin - (-25)) < 0.1);
    assert(std::abs(ymin - (-25)) < 0.1);
}

int main() {
    test_multi_sheet();
    test_alignment_and_bias();
    std::cout << "✨ All Pack tests passed!" << std::endl;
    return 0;
}
