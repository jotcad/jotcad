#include "test_base.h"
#include "../ops/pack_op.h"
#include "packaide_engine.h"

using namespace jotcad::geo;
using namespace pack;

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
    sheets_group.components.push_back(create_box(&vfs, 120, 120));
    sheets_group.components.push_back(create_box(&vfs, 120, 120));

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
        if (comp.tags.contains("sheet") && comp.tags["sheet"] == 1.0) {
            found_sheet_0 = true;
            total_placed_parts += (int)comp.components.size() - 1;
        } else if (comp.tags.contains("sheet") && comp.tags["sheet"] == 2.0) {
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

    // Check Top-Left Bias
    // With spacing 0, it should be exactly at the top-left corner.
    // Sheet x is [-25, 25], Sheet y is [-25, 25].
    // Top-Left corner is (-25, 25).
    // Part size is 10x10.
    // So part should be at [-25, -15] x [15, 25].
    
    std::cout << "  - Verifying Top-Left Bias..." << std::endl;
    assert(std::abs(xmin - (-25)) < 0.1);
    assert(std::abs(ymax - 25) < 0.1);
}

void test_geometric_nesting() {
    std::cout << "Testing Pure Geometric Nesting (Russian Doll)..." << std::endl;

    // 1. 82x82 Sheet (slightly larger than Part A to allow valid placement space)
    packaide::Sheet sheet = packaide::Sheet::rectangle(packaide::FT(82.0), packaide::FT(82.0));

    // 2. Part A: 80x80 Square with a 40x40 Hole
    packaide::Polygon_2 outer;
    outer.push_back(packaide::Point_2(0, 0));
    outer.push_back(packaide::Point_2(80, 0));
    outer.push_back(packaide::Point_2(80, 80));
    outer.push_back(packaide::Point_2(0, 80));

    packaide::Polygon_2 hole;
    hole.push_back(packaide::Point_2(20, 20));
    hole.push_back(packaide::Point_2(20, 60));
    hole.push_back(packaide::Point_2(60, 60));
    hole.push_back(packaide::Point_2(60, 20));

    packaide::Polygon_with_holes_2 pwh_A(outer);
    pwh_A.add_hole(hole);

    PackaideEngine::PartInfo info_A;
    info_A.original_index = 0;
    info_A.cgal_poly = pwh_A;
    info_A.area = packaide::FT(80*80 - 40*40);
    info_A.xmin_off = 0; info_A.ymin_off = 0;

    // 3. Part B: 20x20 Solid Square (Should go in the hole)
    packaide::Polygon_2 p_B;
    p_B.push_back(packaide::Point_2(0, 0));
    p_B.push_back(packaide::Point_2(20, 0));
    p_B.push_back(packaide::Point_2(20, 20));
    p_B.push_back(packaide::Point_2(0, 20));

    PackaideEngine::PartInfo info_B;
    info_B.original_index = 1;
    info_B.cgal_poly = packaide::Polygon_with_holes_2(p_B);
    info_B.area = packaide::FT(400.0);
    info_B.xmin_off = 0; info_B.ymin_off = 0;

    std::vector<PackaideEngine::PartInfo> parts = { info_A, info_B };

    PackaideEngine::Config config;
    config.margin = packaide::FT(0.0);
    config.spacing = packaide::FT(0.0);
    config.simplification_tolerance = packaide::FT(0.0);

    auto placements = PackaideEngine::pack_geometric(parts, sheet, config);

    assert(placements.size() == 2);
    bool found_in_hole = false;
    for (const auto& p : placements) {
        if (p.original_index == 1) {
            // Sheet is 82x82, Part A (80x80) placed at (0, 2) due to Top-Left bias.
            // Translated hole is [20, 60] x [22, 62].
            // For 20x20 Part B, top-left anchor in this translated hole is (20, 42).
            if (p.x == packaide::FT(20) && p.y == packaide::FT(42)) found_in_hole = true;
        }
    }
    assert(found_in_hole);
    std::cout << "  - SUCCESS: Nesting verified." << std::endl;
}

void test_simplification() {
    std::cout << "Testing Standard Polyline Simplification..." << std::endl;

    packaide::Polygon_2 jagged;
    jagged.push_back(packaide::Point_2(0, 0));
    jagged.push_back(packaide::Point_2(5, 0.1));  // Nearly collinear
    jagged.push_back(packaide::Point_2(10, 0));
    jagged.push_back(packaide::Point_2(10, 10));
    jagged.push_back(packaide::Point_2(0, 10));

    packaide::Polygon_with_holes_2 pwh(jagged);
    
    // With 5.0 tolerance, the (5, 0.1) point should be removed
    auto simplified = PackaideEngine::simplify_pwh(pwh, packaide::FT(5.0));
    
    std::cout << "  - Vertices: " << jagged.size() << " -> " << simplified.outer_boundary().size() << std::endl;
    assert(simplified.outer_boundary().size() < jagged.size());
    std::cout << "  - SUCCESS: Simplification verified." << std::endl;
}

void test_item_support() {
    MockVFS vfs("pack_item");
    pack_init(&vfs);
    std::cout << "Testing 'item' Type Support..." << std::endl;

    // 1. Create a parent shape representing a rigid 'item'
    Shape item;
    item.tags["type"] = "item";

    // 2. Add a child box component (the item's nested geometry)
    Shape child = create_box(&vfs, 20, 20);
    child.tf = Matrix::translate(FT(5), FT(5), FT(0)); // relative offset inside item
    item.components.push_back(child);

    // 3. Create a 50x50 Sheet
    Shape sheet = create_box(&vfs, 50, 50);

    // 4. Pack
    fs::Selector sel("jot/pack");
    sel.parameters["$in"] = item;
    sel.parameters["sheet"] = sheet;
    sel.parameters["spacing"] = 2.0;
    sel.parameters["margin"] = 0.0;
    sel.output = "$out";

    Processor::execute(&vfs, sel);

    // 5. Verify the pack result
    Shape out = vfs.read<Shape>(sel);
    assert(out.tags["type"] == "group");
    assert(out.components.size() == 1); // 1 sheet group
    
    Shape sheet_grp = out.components[0];
    // Components of sheet_grp: [0] = sheet background, [1] = placed part
    assert(sheet_grp.components.size() == 2);
    
    Shape placed_item = sheet_grp.components[1];
    // Check that the parent 'item' is returned intact, and not flattened!
    assert(placed_item.tags.contains("type") && placed_item.tags["type"] == "item");
    assert(placed_item.components.size() == 1);
    
    // Check relative nested transforms (under Independent Matrix Mandate, all transforms are absolute world space)
    auto nested = placed_item.components[0];
    double tx = CGAL::to_double(nested.tf.t.cartesian(0, 3));
    double ty = CGAL::to_double(nested.tf.t.cartesian(1, 3));
    
    double p_tx = CGAL::to_double(placed_item.tf.t.cartesian(0, 3));
    double p_ty = CGAL::to_double(placed_item.tf.t.cartesian(1, 3));
    
    // The relative offset within the item (5.0, 5.0) should be perfectly preserved in absolute coordinates!
    assert(std::abs(tx - p_tx - 5.0) < 0.001);
    assert(std::abs(ty - p_ty - 5.0) < 0.001);

    std::cout << "  - SUCCESS: Item support verified." << std::endl;
}

int main() {
    test_multi_sheet();
    test_alignment_and_bias();
    test_geometric_nesting();
    test_simplification();
    test_item_support();
    std::cout << "✨ All Pack tests passed!" << std::endl;
    return 0;
}
