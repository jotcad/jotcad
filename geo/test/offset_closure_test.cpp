#include "test_base.h"
#include "../box_op.h"
#include "../offset_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    std::cout << "Testing Offset Topological Closure..." << std::endl;
    MockVFS vfs;

    // SCENARIO 1: Positive offset fills a small hole
    {
        std::cout << "  - Testing Hole Filling (Positive Offset)..." << std::endl;
        Shape base;
        BoxOp<>::execute(&vfs, {50.0}, {50.0}, {0.0}, base);
        
        Shape hole;
        BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, hole);
        hole.tf = Matrix::translate(20, 20, 0).to_vec();

        Shape holed_box;
        CutOp<>::execute(&vfs, base, {hole}, holed_box);
        
        // Offset by 6mm (radius). Since hole is 10mm wide, a 6mm radius expansion 
        // should fill the hole entirely (10 - 2*6 < 0).
        Shape filled;
        OffsetOp<>::execute(&vfs, holed_box, 6.0, filled);
        
        Geometry geo = vfs.read_geo(filled.geometry);
        // Expect 1 face, 1 loop (Outer only)
        assert(geo.faces.size() == 1);
        assert(geo.faces[0].loops.size() == 1);
        std::cout << "    ✅ Hole filled correctly." << std::endl;
    }

    // SCENARIO 2: Negative offset collapses a small part
    {
        std::cout << "  - Testing Part Collapse (Negative Offset)..." << std::endl;
        Shape small;
        BoxOp<>::execute(&vfs, {10.0}, {10.0}, {0.0}, small);
        
        // Offset by -6mm (radius). Since box is 10mm wide, it should disappear.
        Shape collapsed;
        OffsetOp<>::execute(&vfs, small, -6.0, collapsed);
        
        // In JOT, if a geometry collapses entirely, it should probably return 
        // a Shape with no geometry or an empty mesh.
        if (collapsed.geometry.has_value()) {
            Geometry geo = vfs.read_geo(collapsed.geometry);
            assert(geo.faces.empty());
        }
        std::cout << "    ✅ Part collapsed correctly." << std::endl;
    }

    std::cout << "✅ Offset Closure PASS" << std::endl;
    return 0;
}
