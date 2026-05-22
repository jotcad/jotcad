#include "test_base.h"
#include "pack/packaide_engine.h"

using namespace jotcad::geo;

int main() {
    MockVFS* vfs = new MockVFS("packaide");

    // 1. Create two identical squares
    Geometry box_part;
    box_part.vertices = {
        {FT(0), FT(0), FT(0)}, {FT(247), FT(0), FT(0)}, {FT(247), FT(247), FT(0)}, {FT(0), FT(247), FT(0)}
    };
    box_part.faces.push_back({{{0, 1, 2, 3}}});
    fs::CID hex_cid = vfs->materialize(box_part);

    // 2. Create a large sheet (Box)
    Geometry sheet_geo;
    sheet_geo.vertices = {
        {FT(0), FT(0), FT(0)}, {FT(1000), FT(0), FT(0)}, {FT(1000), FT(1000), FT(0)}, {FT(0), FT(1000), FT(0)}
    };
    sheet_geo.faces.push_back({{{0, 1, 2, 3}}});
    fs::CID sheet_cid = vfs->materialize(sheet_geo);

    // 3. Setup Packing
    pack::PackaideEngine::Config config;
    config.spacing = 10.0;
    config.margin = 5.0;

    Shape p1; p1.geometry = hex_cid;
    Shape p2; p2.geometry = hex_cid;
    std::vector<Shape> parts = {p1, p2};

    Shape s1; s1.geometry = sheet_cid;
    std::vector<Shape> sheets = {s1};

    // 4. Run Packaide Engine
    auto result = pack::PackaideEngine::pack(vfs, parts, sheets, config);

    std::cout << "Unplaced parts: " << result.unplaced.size() << std::endl;
    if (result.bins.size() > 0) {
        const auto& bin = result.bins[0];
        std::cout << "Placed components: " << bin.components.size() << std::endl;

        if (bin.components.size() == 2) {
            auto tf1 = bin.components[0].tf;
            auto tf2 = bin.components[1].tf;

            double tx1 = CGAL::to_double(tf1.t.cartesian(0, 3));
            double ty1 = CGAL::to_double(tf1.t.cartesian(1, 3));
            double tx2 = CGAL::to_double(tf2.t.cartesian(0, 3));
            double ty2 = CGAL::to_double(tf2.t.cartesian(1, 3));

            std::cout << "Part 1 translation: " << tx1 << ", " << ty1 << std::endl;
            std::cout << "Part 2 translation: " << tx2 << ", " << ty2 << std::endl;

            double dx = std::abs(tx1 - tx2);
            double dy = std::abs(ty1 - ty2);
            double dist = std::sqrt(dx*dx + dy*dy);
            std::cout << "Distance between parts: " << dist << std::endl;

            if (dist < 200.0) {
                std::cout << "FAILURE: Parts are overlapping or too close!" << std::endl;
                return 1;
            } else {
                std::cout << "SUCCESS: Parts are correctly separated." << std::endl;
            }
        } else {
            std::cout << "FAILURE: Expected 2 placed parts, got " << bin.components.size() << std::endl;
            return 1;
        }
    } else {
        std::cout << "FAILURE: No bins returned." << std::endl;
        return 1;
    }

    return 0;
}
