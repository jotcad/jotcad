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

    // 3. Regression: Projective Stacking Bug (at + eachFace + and)
    {
        std::cout << "  - Testing at() + eachFace() + and() Distribution..." << std::endl;
        
        fs::Selector box_sel = fs::Selector("jot/Box", {{"width", 10.0}, {"height", 10.0}, {"depth", 10.0}}).with_output("$out");
        Shape box = vfs.read<Shape>(box_sel);

        // Fixed: size must be an array for jot:numbers
        fs::Selector tri_sel = fs::Selector("jot/Triangle/equilateral", {{"size", {5.0}}}).with_output("$out");
        Shape tri = vfs.read<Shape>(tri_sel);

        // Recipe: and(Triangle(5))
        fs::Selector and_sel = fs::Selector("jot/and", {{"shapes", nlohmann::json::array({vfs.materialize(tri).value})}});
        
        // Target: eachFace() needs $in to be provided
        fs::Selector target_sel = fs::Selector("jot/eachFace", {{"$in", vfs.materialize(box).value}}).with_output("$out");

        // Operation: Box.at(eachFace(), and(Triangle(5)))
        fs::Selector at_sel = fs::Selector("jot/at", {
            {"$in", vfs.materialize(box).value},
            {"target", target_sel.to_json()},
            {"op", and_sel.to_json()}
        }).with_output("$out");

        Shape result = vfs.read<Shape>(at_sel);
        
        // The result should be a nested structure. 
        // With sibling grouping, it should be a chain of Groups where each Group 
        // contains the accumulated subject and one new Triangle sibling.
        
        std::vector<Matrix> tri_tfs;
        std::function<void(const Shape&, Matrix)> collect_triangles = [&](const Shape& s, Matrix parent_tf) {
            Matrix world_tf = parent_tf * s.tf;
            // The Triangle we added is a 'surface' component with identity birth tf
            if (s.tags.value("type", "") == "surface" && s.components.empty()) {
                tri_tfs.push_back(world_tf);
            }
            for (const auto& c : s.components) collect_triangles(c, world_tf);
        };
        collect_triangles(result, Matrix::identity());

        std::cout << "    Found " << tri_tfs.size() << " Triangle siblings." << std::endl;
        if (tri_tfs.size() < 6) {
            std::cerr << "FAIL: Expected at least 6 triangles, found " << tri_tfs.size() << std::endl;
            return 1;
        }

        // Check if any triangle transforms are identical (sign of stacking)
        for (size_t i = 0; i < tri_tfs.size(); ++i) {
            for (size_t j = i + 1; j < tri_tfs.size(); ++j) {
                if (tri_tfs[i].s == tri_tfs[j].s) {
                    std::cerr << "FAIL: Detected stacked triangles! Index " << i << " and " << j << " have identical matrices." << std::endl;
                    std::cerr << "      Matrix: " << tri_tfs[i].s << std::endl;
                    return 1;
                }
            }
        }
        std::cout << "    [PASS] All triangles are uniquely positioned." << std::endl;
    }

    std::cout << "[SUCCESS] EachFace test passed." << std::endl;
    return 0;
}
