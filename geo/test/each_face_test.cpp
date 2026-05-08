#include "test_base.h"
#include "data/shape.h"
#include <iomanip>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("each_face_test");
    register_all_ops(&vfs);

    std::cout << "[Test] EachFace Patch Merging..." << std::endl;

    // 1. Box Test (6 faces)
    {
        std::cout << "  - Testing Box(10,10,10)..." << std::endl;
        fs::Selector box_sel = fs::Selector("jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}).with_output("$out");
        Shape box = vfs.read<Shape>(box_sel);
        
        fs::Selector each_face_sel = fs::Selector("jot/eachFace", {{"$in", box}}).with_output("$out");
        Shape faces_group = vfs.read<Shape>(each_face_sel);
        
        std::cout << "    Components: " << faces_group.components.size() << " (Expected: 6)" << std::endl;
        if (faces_group.components.size() != 6) {
            std::cerr << "FAIL: Expected 6 face components for Box, got " << faces_group.components.size() << std::endl;
            return 1;
        }
    }

    // 2. Triangulated Patch Merger (Hexagon clipped)
    {
        std::cout << "  - Testing Triangulated Hexagon Merger..." << std::endl;
        
        fs::Selector hex_sel = fs::Selector("jot/Hexagon/radius", {{"radius", 10.0}, {"turns", 0.0}}).with_output("$out");
        Shape hex = vfs.read<Shape>(hex_sel);
        
        fs::Selector box_tool_sel = fs::Selector("jot/Box", {{"width", 50.0}, {"height", 50.0}, {"depth", 50.0}}).with_output("$out");
        Shape box_tool = vfs.read<Shape>(box_tool_sel);

        fs::Selector clip_sel = fs::Selector("jot/clip", {{"$in", hex}, {"tool", box_tool}}).with_output("$out");
        Shape clipped = vfs.read<Shape>(clip_sel);
        
        Geometry clipped_geo = vfs.read<Geometry>(clipped.geometry.value());
        std::cout << "    Clipped Geo: Triangles=" << clipped_geo.triangles.size() << ", Faces=" << clipped_geo.faces.size() << std::endl;
        
        // Debug: what are the faces?
        for (const auto& f : clipped_geo.faces) {
            std::cout << "      Face Loop size: " << f.loops[0].size() << std::endl;
        }

        fs::Selector each_face_sel = fs::Selector("jot/eachFace", {{"$in", clipped}}).with_output("$out");
        Shape faces_group = vfs.read<Shape>(each_face_sel);
        
        std::cout << "    Merged Components: " << faces_group.components.size() << " (Expected: 1)" << std::endl;
        if (faces_group.components.size() != 1) {
            std::cerr << "FAIL: Expected 1 merged face component for triangulated Hexagon, got " << faces_group.components.size() << std::endl;
            return 1;
        }
        
        Geometry merged_geo = vfs.read<Geometry>(faces_group.components[0].geometry.value());
        std::cout << "    Merged Face Loops: " << merged_geo.faces[0].loops[0].size() << " (Expected: 6 for hexagon)" << std::endl;
        if (merged_geo.faces[0].loops[0].size() != 6) {
             std::cerr << "FAIL: Merged polygon should have 6 vertices, got " << merged_geo.faces[0].loops[0].size() << std::endl;
             return 1;
        }
    }

    std::cout << "[SUCCESS] EachFace test passed." << std::endl;
    return 0;
}
