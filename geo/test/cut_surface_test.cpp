#include "test_base.h"
#include "../box_op.h"
#include "../cut_op.h"

using namespace jotcad::geo;

int main() {
    MockVFS vfs("cut_surface");
    
    std::cout << "Testing Cut (Surface by Surface, Coplanar at Z=5)..." << std::endl;
    
    // 1. Create a 10x10 Square (Surface) at Z=5
    // Bounds: [-5, 5] in X/Y, Z=5
    Geometry target_geo;
    target_geo.vertices = {{FT(-5), FT(-5), FT(5)}, {FT(5), FT(-5), FT(5)}, {FT(5), FT(5), FT(5)}, {FT(-5), FT(5), FT(5)}};
    target_geo.faces.push_back({{{0, 1, 2, 3}}});
    
    Shape target_shape;
    target_shape.geometry = vfs.materialize<Geometry>(target_geo);
    target_shape.tf = Matrix::identity().to_vec();

    // 2. Create a 4x4 Square (Tool) at Z=5
    // Bounds: [-2, 2] in X/Y, Z=5
    Geometry tool_geo;
    tool_geo.vertices = {{FT(-2), FT(-2), FT(5)}, {FT(2), FT(-2), FT(5)}, {FT(2), FT(2), FT(5)}, {FT(-2), FT(2), FT(5)}};
    tool_geo.faces.push_back({{{0, 1, 2, 3}}});

    Shape tool_shape;
    tool_shape.geometry = vfs.materialize<Geometry>(tool_geo);
    tool_shape.tf = Matrix::identity().to_vec();
    
    // 3. Perform Cut
    fs::Selector cut_sel = {"jot/cut/surface", {}};
    CutOp<>::execute(&vfs, cut_sel, target_shape, {tool_shape}, false);
    
    Shape out = vfs.read<Shape>(cut_sel);
    Geometry res = vfs.read<Geometry>(out.geometry.value());

    std::cout << "  Resulting surface has " << res.faces.size() << " faces and " << res.vertices.size() << " vertices." << std::endl;

    // If it worked, we should have a hole in the middle.
    // The number of vertices should have increased to account for the internal boundary.
    for (const auto& f : res.faces) {
        std::cout << "  Face loop size: " << f.loops[0].size() << std::endl;
    }

    // Check if the "hole" is present by verifying vertex counts or specific coordinates.
    // A successful 2D-style cut on a 3D plane would typically produce a face with a hole (2 loops)
    // or a subdivided set of triangles.
    
    bool has_hole = false;
    for (const auto& f : res.faces) {
        if (f.loops.size() > 1) has_hole = true;
    }
    
    if (has_hole) {
        std::cout << "  ✅ Result has a hole loop." << std::endl;
    } else {
        std::cout << "  ❌ Result has NO hole loop (might be triangulated or failed)." << std::endl;
    }

    std::cout << "✅ Surface Cut Test Finished" << std::endl;
    return 0;
}
