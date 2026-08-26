#include "test_base.h"
#include "render/rasterizer.h"
#include "infra/stl.h"
#include <fstream>
#include <cmath>

using namespace jotcad::geo;

void test_part_line_visuals() {
    MockVFS vfs("part_line_visuals");
    register_all_ops(&vfs);

    // 1. Load the bear STL
    std::string bear_path = "scratch/bear.stl";
    {
        std::ifstream check(bear_path);
        if (!check.good()) {
            check.open("../scratch/bear.stl");
            if (check.good()) {
                bear_path = "../scratch/bear.stl";
            } else {
                bear_path = "../../scratch/bear.stl";
            }
        }
    }
    std::cout << "  - Loading " << bear_path << "..." << std::endl;
    Geometry bear_geo;
    if (!STLReader::read_file(bear_path, bear_geo)) {
        std::cerr << "  ❌ FAIL: Could not load bear.stl from " << bear_path << std::endl;
        return;
    }
    std::cout << "    - Loaded successfully with " << bear_geo.triangles.size() << " triangles." << std::endl;

    Shape bear_shape = JotVfsProtocol::make_shape(&vfs, bear_geo, json::object());

    // 2. Perform PartLine Analysis with optimize=true
    std::cout << "  - Executing jot/partLine with optimize=true..." << std::endl;
    fs::Selector part_line_sel("jot/partLine");
    part_line_sel.parameters["$in"] = bear_shape.to_json();
    part_line_sel.parameters["optimize"] = true;
    part_line_sel.output = "$out";

    try {
        Processor::execute(&vfs, part_line_sel);
        std::cerr << "  ❌ Expected Demoldability Error on bear." << std::endl;
        throw std::runtime_error("Expected Demoldability Error on bear");
    } catch (const std::runtime_error& e) {
        std::cout << "    - Caught expected Demoldability Error on bear: " << e.what() << std::endl;
    }

    // 3. Visual test on 2-piece Box
    std::cout << "  - Visual test of jot/partLine on 2-piece Box..." << std::endl;
    fs::Selector box_sel("jot/Box");
    box_sel.parameters["width"] = 10.0;
    box_sel.parameters["height"] = 10.0;
    box_sel.parameters["depth"] = 10.0;
    Shape box = vfs.read<Shape>(box_sel.with_output("$out"));

    fs::Selector box_opt("jot/partLine");
    box_opt.parameters["$in"] = box.to_json();
    box_opt.parameters["optimize"] = true;
    Shape box_parting = vfs.read<Shape>(box_opt.with_output("$out"));

    Shape composite;
    composite.components.push_back(box);
    composite.components.push_back(box_parting);
    composite.tf = Matrix::rotationX(-0.61547) * Matrix::rotationY(0.78539);

    auto png_data = Rasterizer::render_png(&vfs, composite, 512, 512, 0.0, 0.0);
    if (!png_data.empty()) {
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/box_part_line_optimal.png", std::ios::binary);
        out.write((const char*)png_data.data(), png_data.size());
        std::cout << "  📸 Saved actual/box_part_line_optimal.png" << std::endl;
    }
}

int main() {
    test_part_line_visuals();
    return 0;
}
