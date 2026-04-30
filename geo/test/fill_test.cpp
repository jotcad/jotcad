#include "test_base.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("fill");
    register_all_ops(&vfs);
    
    std::cout << "Testing Fill Operator..." << std::endl;
    
    // 1. Fill a simple square loop
    Geometry square;
    square.vertices = {{FT(0), FT(0), FT(0)}, {FT(10), FT(0), FT(0)}, {FT(10), FT(10), FT(0)}, {FT(0), FT(10), FT(0)}};
    square.segments = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    
    Shape s_in = JotVfsProtocol::make_shape(&vfs, square, {{"type", "path"}});
    fs::Selector fill_addr = fs::Selector{"jot/fill", {{"$in", s_in}, {"rule", "odd"}}}.with_output("$out");
    
    Processor::execute(&vfs, fill_addr);
    Shape s_out = vfs.read<Shape>(fill_addr);
    Geometry geo_out = vfs.read<Geometry>(s_out.geometry.value());
    
    std::cout << "  - Square Fill: " << geo_out.faces.size() << " faces." << std::endl;
    assert(geo_out.faces.size() == 1);
    assert(geo_out.faces[0].loops[0].size() == 4);

    // 2. Fill nested loops (Even-Odd)
    Geometry nested;
    // Outer square
    nested.vertices = {{FT(0), FT(0), FT(0)}, {FT(20), FT(0), FT(0)}, {FT(20), FT(20), FT(0)}, {FT(0), FT(20), FT(0)}};
    nested.segments = {{0, 1}, {1, 2}, {2, 3}, {3, 0}};
    // Inner square (hole)
    nested.vertices.push_back({FT(5), FT(5), FT(0)});
    nested.vertices.push_back({FT(15), FT(5), FT(0)});
    nested.vertices.push_back({FT(15), FT(15), FT(0)});
    nested.vertices.push_back({FT(5), FT(15), FT(0)});
    nested.segments.push_back({4, 5});
    nested.segments.push_back({5, 6});
    nested.segments.push_back({6, 7});
    nested.segments.push_back({7, 4});

    Shape s_nested = JotVfsProtocol::make_shape(&vfs, nested, {{"type", "path"}});
    fs::Selector fill_nested_addr = fs::Selector{"jot/fill", {{"$in", s_nested}, {"rule", "odd"}}}.with_output("$out");
    
    Processor::execute(&vfs, fill_nested_addr);
    Shape s_nested_out = vfs.read<Shape>(fill_nested_addr);
    Geometry geo_nested_out = vfs.read<Geometry>(s_nested_out.geometry.value());
    
    // In Even-Odd, nested loops should result in a single face with a hole
    std::cout << "  - Nested Fill (Even-Odd): " << geo_nested_out.faces.size() << " faces." << std::endl;
    assert(geo_nested_out.faces.size() == 1);
    assert(geo_nested_out.faces[0].loops.size() == 2); // Outer + Hole

    std::cout << "✅ ALL Fill Tests Passed" << std::endl;
    return 0;
}
