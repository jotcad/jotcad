#include <iostream>
#include <vector>
#include <cassert>
#include "data/geometry.h"
#include "data/shape.h"
#include "pack/engine.h"
#include "vfs_node.h"

using namespace jotcad;
using namespace geo;

/**
 * UNIT TEST: pack::Engine
 * Verifies that multiple 2D shapes are packed into a sheet.
 */
int main() {
    std::cout << "Unit Test: pack::Engine" << std::endl;

    // 0. Setup VFS
    fs::VFSNode::Config vfs_cfg;
    vfs_cfg.storage_dir = "./.vfs_storage_pack_test";
    fs::VFSNode vfs(vfs_cfg);

    // 1. Create a set of triangles
    std::vector<Shape> shapes;
    for (int i = 0; i < 10; ++i) {
        Shape s;
        Geometry geo;
        geo.vertices = {
            {FT(0), FT(0), FT(0)}, {FT(20), FT(0), FT(0)}, {FT(0), FT(20), FT(0)}
        };
        Geometry::Face face;
        face.loops = {{0, 1, 2}};
        geo.faces = {face};
        
        // Materialize geometry to get a CID
        s.geometry = vfs.materialize<Geometry>(geo);
        s.tf = Matrix::identity();
        shapes.push_back(std::move(s));
    }

    // 2. Pack them into a small sheet (e.g., 100x100)
    pack::Engine::Config config;
    config.sheet_width = 100.0;
    config.sheet_height = 100.0;
    config.spacing = 2.0;
    config.margin = 5.0;

    std::cout << "  - Packing " << shapes.size() << " triangles..." << std::endl;
    try {
        pack::Engine::pack(&vfs, shapes, config);
    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] Packing failed: " << e.what() << std::endl;
        return 1;
    }

    // 3. Verify that they were moved
    int moved = 0;
    for (size_t i = 0; i < shapes.size(); ++i) {
        // If the translation is non-zero, it was moved.
        auto tr = shapes[i].tf.transform(EK::Point_3(0, 0, 0));
        if (std::abs(CGAL::to_double(tr.x())) > 0.001 || std::abs(CGAL::to_double(tr.y())) > 0.001) {
            moved++;
        }
        std::cout << "  - Triangle " << i << " moved to: " 
                  << CGAL::to_double(tr.x()) << ", " << CGAL::to_double(tr.y()) << std::endl;
    }

    assert(moved > 0 && "At least some triangles should have been moved");
    
    std::cout << "✅ Unit Test Passed" << std::endl;
    return 0;
}
