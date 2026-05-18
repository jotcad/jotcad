#include "test_base.h"
#include "../ops/pack_op.h"
#include "../ops/hexagon_op.h"
#include "../ops/box_op.h"

using namespace jotcad::geo;

void repro_overlap() {
    MockVFS vfs("repro_pack");
    pack_init(&vfs);
    hexagon_init(&vfs);
    box_init(&vfs);

    std::cout << "Reproducing Pack Overlap..." << std::endl;

    // 1. Create Hexagon(edgeToEdge=247)
    fs::Selector hex_sel("jot/Hexagon/edgeToEdge");
    hex_sel.parameters["edgeToEdge"] = 247.0;
    hex_sel.output = "$out";
    Processor::execute(&vfs, hex_sel);
    Shape hex = vfs.read<Shape>(hex_sel);

    // 2. Create two of them in a group
    Shape group;
    group.tags["type"] = "group";
    group.components.push_back(hex);
    group.components.push_back(hex);

    // 3. Create Sheet Box(600, 300)
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 600.0;
    box_sel.parameters["height"] = 300.0;
    box_sel.output = "$out";
    Processor::execute(&vfs, box_sel);
    Shape sheet = vfs.read<Shape>(box_sel);

    // 4. Pack
    fs::Selector pack_sel("jot/pack");
    pack_sel.parameters["$in"] = group;
    pack_sel.parameters["sheet"] = sheet;
    pack_sel.parameters["spacing"] = 2.0;
    pack_sel.output = "$out";

    Processor::execute(&vfs, pack_sel);

    Shape out = vfs.read<Shape>(pack_sel);
    
    // 5. Check placements
    assert(out.components.size() >= 1);
    Shape sheet_res = out.components[0];
    
    int parts_found = 0;
    std::vector<Matrix> tfs;
    for (const auto& comp : sheet_res.components) {
        if (comp.geometry.has_value() && comp.geometry != sheet.geometry) {
            parts_found++;
            tfs.push_back(comp.tf);
            std::cout << "  Part " << parts_found << " translation: " 
                      << CGAL::to_double(comp.tf.t.hm(0,3)) << ", " 
                      << CGAL::to_double(comp.tf.t.hm(1,3)) << std::endl;
        }
    }

    if (parts_found < 2) {
        std::cout << "  FAILURE: Only " << parts_found << " parts were placed!" << std::endl;
        return;
    }

    double dx = std::abs(CGAL::to_double(tfs[0].t.hm(0,3)) - CGAL::to_double(tfs[1].t.hm(0,3)));
    double dy = std::abs(CGAL::to_double(tfs[0].t.hm(1,3)) - CGAL::to_double(tfs[1].t.hm(1,3)));
    
    std::cout << "  Distance between parts: dx=" << dx << ", dy=" << dy << std::endl;
    
    if (dx < 10.0 && dy < 10.0) {
        std::cout << "  REPRODUCED: Parts are overlapping!" << std::endl;
    } else {
        std::cout << "  SUCCESS: Parts are NOT overlapping." << std::endl;
    }
}

int main() {
    repro_overlap();
    return 0;
}
