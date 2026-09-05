#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include <iostream>
#include <cassert>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("mold_l_test");
    register_all_ops(&vfs);

    std::cout << "============================================================" << std::endl;
    std::cout << "Testing jot/mold on L-Shaped Solid (Concave / Re-entrant Corner)" << std::endl;
    std::cout << "============================================================" << std::endl;

    // 1. Construct L-bracket solid geometry via JOT operators:
    std::cout << "  - Building exact rational L-bracket geometry..." << std::endl;
    fs::Selector block_sel("jot/Box");
    block_sel.parameters["width"] = 20.0;
    block_sel.parameters["height"] = 20.0;
    block_sel.parameters["depth"] = 10.0;
    Shape block_shape = vfs.read<Shape>(block_sel.with_output("$out"));

    fs::Selector notch_sel("jot/Box");
    notch_sel.parameters["width"] = nlohmann::json::array({0.0, 11.0});
    notch_sel.parameters["height"] = nlohmann::json::array({0.0, 11.0});
    notch_sel.parameters["depth"] = nlohmann::json::array({-6.0, 6.0});
    Shape notch_shape = vfs.read<Shape>(notch_sel.with_output("$out"));

    fs::Selector cut_sel("jot/cut");
    cut_sel.parameters["$in"] = block_shape.to_json();
    cut_sel.parameters["tools"] = nlohmann::json::array({notch_shape.to_json()});
    Shape l_shape = vfs.read<Shape>(cut_sel.with_output("$out"));

    // 2. Execute jot/mold decomposition with padding=5.0 mm and explode=20.0 mm
    std::cout << "  - Executing jot/mold decomposition (padding=5.0, explode=20.0)..." << std::endl;
    fs::Selector mold_sel("jot/mold");
    mold_sel.parameters["$in"] = l_shape.to_json();
    mold_sel.parameters["padding"] = 5.0;
    mold_sel.parameters["explode"] = 20.0;
    mold_sel.parameters["draft"] = 0.0;
    mold_sel.parameters["kiss"] = "weld";
    mold_sel.parameters["kiss_width"] = 0.01;
    mold_sel.output = "$out";

    Processor::execute(&vfs, mold_sel);
    Shape mold_result = vfs.read<Shape>(mold_sel);

    // 3. Verify Assembly Components & Demoldability
    std::cout << "  - Verifying extracted mold components..." << std::endl;
    assert(mold_result.components.size() >= 3 && "Must have at least 1 model + 2 mold pieces!");

    int piece_count = 0;
    bool found_model = false;

    for (const auto& comp : mold_result) {
        if (!comp.has_tag("mold/role")) continue;
        std::string role = comp.tags["mold/role"];

        if (role == "model") {
            found_model = true;
            std::cout << "    * Central model shape preserved." << std::endl;
        } else if (role == "piece") {
            piece_count++;
            assert(comp.has_tag("mold/piece"));
            assert(comp.has_tag("mold/pull_vector"));
            std::cout << "    * Mold piece #" << comp.tags["mold/piece"]
                      << " pull_vector=[" << comp.tags["mold/pull_vector"] << "]"
                      << " color=" << comp.tags["color"] << std::endl;

            assert(comp.geometry.has_value());
            Geometry piece_geo = vfs.read<Geometry>(comp.geometry.value());
            assert(!piece_geo.vertices.empty() && (!piece_geo.faces.empty() || !piece_geo.triangles.empty()));
            size_t face_count = piece_geo.faces.size() + piece_geo.triangles.size();
            std::cout << "      Faces: " << face_count << " Vertices: " << piece_geo.vertices.size() << std::endl;
        }
    }

    assert(found_model && "Central model was not found in mold assembly!");
    assert(piece_count >= 2 && "L-bracket requires at least 2 certified mold pieces!");
    std::cout << "  - Total mold pieces extracted: " << piece_count << std::endl;

    std::cout << "✅ mold_l_test PASSED successfully." << std::endl;
    return 0;
}
