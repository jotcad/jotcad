#include "test_base.h"
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;

int main() {
    MockVFS vfs("path_test");
    register_all_ops(&vfs);

    // Create some points
    Shape p1; p1.tf = Matrix::translate(0, 0, 0);
    Shape p2; p2.tf = Matrix::translate(10, 0, 0);
    Shape p3; p3.tf = Matrix::translate(10, 10, 0);
    Shape p4; p4.tf = Matrix::translate(0, 10, 0);

    nlohmann::json tool_shapes = {p2.to_json(), p3.to_json(), p4.to_json()};

    // Test Link (open) - Straight (smooth=false)
    {
        fs::Selector sel = fs::Selector{"jot/link", {
            {"$in", p1.to_json()}, 
            {"tools", tool_shapes}, 
            {"smooth", false}
        }}.with_output("$out");
        
        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Link (straight) vertices: " << geo.vertices.size() << " (expected 4)" << std::endl;
        assert(geo.vertices.size() == 4);
    }

    // Test Link (open) - Smooth (smooth=true, zag=0.1 -> 10 steps/seg)
    {
        fs::Selector sel = fs::Selector{"jot/link", {
            {"$in", p1.to_json()}, 
            {"tools", tool_shapes}, 
            {"smooth", true}, 
            {"zag", 0.1}
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Link (smooth zag 0.1) vertices: " << geo.vertices.size() << " (expected 31)" << std::endl;
        assert(geo.vertices.size() == 31);
    }

    // Test Loop (closed) - Straight
    {
        fs::Selector sel = fs::Selector{"jot/loop", {
            {"$in", p1.to_json()}, 
            {"tools", tool_shapes}, 
            {"smooth", false}
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Loop (straight) vertices: " << geo.vertices.size() << " (expected 4)" << std::endl;
        assert(geo.vertices.size() == 4);
        assert(geo.segments.size() == 4);
    }

    // Test Loop (closed) - Smooth
    {
        fs::Selector sel = fs::Selector{"jot/loop", {
            {"$in", p1.to_json()}, 
            {"tools", tool_shapes}, 
            {"smooth", true}, 
            {"zag", 0.1}
        }}.with_output("$out");

        Processor::execute(&vfs, sel);
        Shape out = vfs.read<Shape>(sel);
        Geometry geo = vfs.read<Geometry>(out.geometry.value());
        
        std::cout << "Loop (smooth zag 0.1) vertices: " << geo.vertices.size() << " (expected 40)" << std::endl;
        assert(geo.vertices.size() == 40);
        assert(geo.segments.size() == 40);
    }

    std::cout << "Path test PASSED" << std::endl;

    return 0;
}
