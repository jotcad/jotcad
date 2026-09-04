#include "test_base.h"
#include "protocols.h"
#include "processor.h"
#include "mold/types.h"
#include "mold/repair.h"
#include "mold/assembly.h"
#include "fix/assert_mesh.h"
#include "boolean/engine.h"
#include "boolean/corefine.h"
#include <CGAL/Polygon_mesh_processing/measure.h>
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

    // 1. Construct L-bracket solid geometry:
    // Base block: 20 x 20 x 10, from (-10, -10, -5) to (10, 10, 5) [Vol = 4000]
    // Corner notch: 11 x 11 x 12, from (0, 0, -6) to (11, 11, 6)   [Vol = 1000 removed]
    // The notch removes the (+X, +Y) quadrant, creating a classic L-bracket with a 90° reflex inside corner.
    std::cout << "  - Building exact rational L-bracket geometry..." << std::endl;
    Geometry block_geo = mold::build_box_geo(FT(-10), FT(10), FT(-10), FT(10), FT(-5), FT(5));
    Geometry notch_geo = mold::build_box_geo(FT(0), FT(11), FT(0), FT(11), FT(-6), FT(6));
    mold::ExactMesh block_mesh = boolean::Engine::geometry_to_mesh(block_geo);
    mold::ExactMesh notch_mesh = boolean::Engine::geometry_to_mesh(notch_geo);

    mold::ExactMesh l_mesh;
    bool ok_cut = boolean::corefine_difference(block_mesh, notch_mesh, l_mesh, fix::KissMode::WELD, mold::pinch_bridge_width_ft(), "L-bracket cut");
    assert(ok_cut && "Failed to construct L-bracket via corefine_difference!");
    fix::assert_well_formed_mesh(l_mesh, "Constructed L-bracket");

    FT vol = CGAL::Polygon_mesh_processing::volume(l_mesh);
    std::cout << "  - L-bracket volume: " << CGAL::to_double(vol) << " (expected: 3000.0)" << std::endl;
    assert(vol == FT(3000));

    // Convert L-mesh to VFS Shape
    Geometry l_geo = boolean::Engine::mesh_to_geometry(l_mesh);
    Shape l_shape = JotVfsProtocol::make_shape(&vfs, l_geo, {{"type", "closed"}, {"name", "l_bracket"}});

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
    FT total_piece_volume = FT(0);

    for (const auto& comp : mold_result.components) {
        if (!comp.tags.contains("mold/role")) continue;
        std::string role = comp.tags["mold/role"];

        if (role == "model") {
            found_model = true;
            std::cout << "    * Central model shape preserved." << std::endl;
        } else if (role == "piece") {
            piece_count++;
            assert(comp.tags.contains("mold/piece"));
            assert(comp.tags.contains("mold/pull_vector"));
            std::cout << "    * Mold piece #" << comp.tags["mold/piece"]
                      << " pull_vector=[" << comp.tags["mold/pull_vector"] << "]"
                      << " color=" << comp.tags["color"] << std::endl;

            assert(comp.geometry.has_value());
            Geometry piece_geo = vfs.read<Geometry>(comp.geometry.value());
            mold::ExactMesh piece_mesh = boolean::Engine::geometry_to_mesh(piece_geo);
            fix::assert_well_formed_mesh(piece_mesh, "Mold piece " + std::to_string(piece_count));

            FT p_vol = CGAL::Polygon_mesh_processing::volume(piece_mesh);
            assert(p_vol > FT(0) && "Mold piece must have positive volume!");
            total_piece_volume += p_vol;
            std::cout << "      Volume: " << CGAL::to_double(p_vol) << " mm^3" << std::endl;
        }
    }

    assert(found_model && "Central model was not found in mold assembly!");
    assert(piece_count >= 2 && "L-bracket requires at least 2 certified mold pieces!");
    std::cout << "  - Total mold pieces extracted: " << piece_count << std::endl;
    std::cout << "  - Total mold pieces volume: " << CGAL::to_double(total_piece_volume) << " mm^3" << std::endl;

    std::cout << "✅ mold_l_test PASSED successfully." << std::endl;
    return 0;
}
