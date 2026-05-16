#include "test_base.h"
#include "cone_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cone");
    register_all_ops(&vfs);

    // 1. Test basic Cone
    {
        fs::Selector sel = fs::Selector{"jot/Cone", {{"diameter", 10.0}, {"height", 20.0}}}.with_output("$out");
        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Cone vertices: " << geo.vertices.size() << std::endl;
        std::cout << "Cone faces: " << geo.faces.size() << std::endl;
        
        assert(geo.vertices.size() > 5);
        // Height 20 centered is -10 to 10. Tip is at X-max = 10.
        assert(geo.vertices[0].x == FT(10));
    }

    // 2. Test Cone/fit
    {
        fs::Selector sel = fs::Selector{"jot/Cone/fit", {{"width", 50.0}, {"height", 20.0}, {"depth", 10.0}}}.with_output("$out");
        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Cone/fit vertices: " << geo.vertices.size() << std::endl;
        // Width 50 centered is -25 to 25. Tip should be at 25.
        assert(geo.vertices[0].x == FT(25));
    }

    // 3. Test Cone/angle
    {
        // 45 degrees angle (0.125 turns), diameter 10 (radius 5) -> height 5.
        // Centered height 5 is -2.5 to 2.5. Tip is at 2.5.
        fs::Selector sel = fs::Selector{"jot/Cone/angle", {{"diameter", 10.0}, {"angle", 0.125}}}.with_output("$out");
        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Cone/angle vertices: " << geo.vertices.size() << std::endl;
        assert(geo.vertices[0].x == FT(2.5));
    }

    std::cout << "✅ ALL Cone Tests Passed" << std::endl;
    return 0;
}
