#include <iostream>
#include <vector>
#include <cassert>
#include "data/geometry.h"
#include "boolean/engine.h"
#include "unfold/clusterer.h"

using namespace jotcad;
using namespace geo;

/**
 * PURE UNIT TEST: unfold::Clusterer
 * Tests the unfolding logic directly on CGAL meshes.
 */
int main() {
    std::cout << "Unit Test: unfold::Clusterer (Pure)" << std::endl;

    // 1. Construct a simple 3D triangle in the XY plane
    Geometry geo;
    geo.vertices = {
        {0, 0, 0}, {10, 0, 0}, {0, 10, 0}
    };
    geo.triangles = {
        {0, 1, 2}
    };

    std::cout << "  - Converting Geometry to ExactMesh..." << std::endl;
    boolean::ExactMesh mesh = boolean::Engine::geometry_to_mesh(geo);
    std::cout << "  - Mesh faces: " << mesh.number_of_faces() << std::endl;

    // 2. Call Clusterer directly
    try {
        std::cout << "  - Calling Clusterer::unfold..." << std::endl;
        std::vector<unfold::UnfoldPatch> patches = unfold::Clusterer::unfold(mesh);
        
        std::cout << "  - Success! Produced " << patches.size() << " patches." << std::endl;
        assert(patches.size() == 1);
        
    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] Caught exception: " << e.what() << std::endl;
        return 1;
    }

    // 4. Test a 3D Cube (12 triangles -> 6 islands)
    std::cout << "  - Testing 3D Cube (12 triangles)..." << std::endl;
    geo.vertices = {
        {0,0,0}, {10,0,0}, {10,10,0}, {0,10,0},
        {0,0,10}, {10,0,10}, {10,10,10}, {0,10,10}
    };
    geo.triangles = {
        {0,1,2}, {0,2,3}, // Bottom
        {4,5,6}, {4,6,7}, // Top
        {0,1,5}, {0,5,4}, // Front
        {1,2,6}, {1,6,5}, // Right
        {2,3,7}, {2,7,6}, // Back
        {3,0,4}, {3,4,7}  // Left
    };
    mesh = boolean::Engine::geometry_to_mesh(geo);
    
    try {
        std::vector<unfold::UnfoldPatch> patches = unfold::Clusterer::unfold(mesh);
        std::cout << "  - Success! Cube produced " << patches.size() << " patches." << std::endl;
        assert(patches.size() == 6 && "Cube should have 6 coplanar islands");
        
        for (const auto& p : patches) {
            assert(p.geometry.faces.size() == 1 && "Each island should be one face");
            assert(p.geometry.vertices.size() == 4 && "Each island should be a square (4 vertices)");

            // Verification: Edge lengths should be exactly 10.0
            for (size_t i = 0; i < p.geometry.vertices.size(); ++i) {
                const auto& v1 = p.geometry.vertices[i];
                const auto& v2 = p.geometry.vertices[(i + 1) % p.geometry.vertices.size()];
                FT dx = v1.x - v2.x;
                FT dy = v1.y - v2.y;
                FT d2 = dx*dx + dy*dy;
                if (CGAL::to_double(d2) < 99.9 || CGAL::to_double(d2) > 100.1) {
                    std::cerr << "  - [FAIL] Edge length mismatch: expected 100 (squared), got " << CGAL::to_double(d2) << std::endl;
                    return 1;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] Cube failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "✅ Unit Test Passed" << std::endl;
    return 0;
}
