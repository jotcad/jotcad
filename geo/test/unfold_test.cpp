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
        
        // New Jigsaw Merge behavior: should ideally produce 1 single net for a cube.
        assert(patches.size() == 1 && "Cube should be unfolded into a single net");
        
        const auto& p = patches[0];
        // 6 faces of 2 triangles each = 12 triangles total.
        // Our Atom welder might have reduced this to 6 square faces.
        // The important part is that the result is a valid 2D net.
        assert(p.geometry.faces.size() >= 1 && "Net should have geometry");
        
        std::cout << "  - Net faces: " << p.geometry.faces.size() << std::endl;

        // Verification: Total area should be 6 * (10*10) = 600
        FT total_area = 0;
        for (const auto& face : p.geometry.faces) {
            // Simplified area check for triangles/quads in XY plane
            if (face.loops.empty()) continue;
            const auto& loop = face.loops[0];
            for (size_t i = 0; i < loop.size(); ++i) {
                const auto& v1 = p.geometry.vertices[loop[i]];
                const auto& v2 = p.geometry.vertices[loop[(i + 1) % loop.size()]];
                total_area = total_area + (v1.x * v2.y - v2.x * v1.y);
            }
        }
        total_area = total_area / FT(2.0);
        if (total_area < FT(0)) total_area = -total_area;

        std::cout << "  - Total net area: " << CGAL::to_double(total_area) << std::endl;
        assert(std::abs(CGAL::to_double(total_area) - 600.0) < 0.1 && "Total area should be 600");

    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] Cube failed: " << e.what() << std::endl;
        return 1;
    }

    // 5. Test minFold suppression on a slightly bent roof (~0.57 degrees)
    std::cout << "  - Testing minFold suppression on slightly bent roof (~0.57 degrees)..." << std::endl;
    geo.vertices = {
        {0, 0, 0}, {10, 0, 0}, {0, 10, 0.05}, {0, -10, 0.05}
    };
    geo.triangles = {
        {0, 1, 2},
        {0, 3, 1}
    };
    mesh = boolean::Engine::geometry_to_mesh(geo);

    try {
        // Run with threshold = 1.0 (should suppress the fold tag)
        std::vector<unfold::UnfoldPatch> patches_suppressed = unfold::Clusterer::unfold(mesh, 1.0);
        assert(patches_suppressed.size() == 1);
        const auto& p_sup = patches_suppressed[0];
        std::cout << "    * minFold = 1.0 -> Edge tags size: " << p_sup.edge_tags.size() << std::endl;
        assert(p_sup.edge_tags.empty() && "Hinge should not be tagged as fold with minFold=1.0");

        // Run with threshold = 0.1 (should NOT suppress the fold tag)
        std::vector<unfold::UnfoldPatch> patches_tagged = unfold::Clusterer::unfold(mesh, 0.1);
        assert(patches_tagged.size() == 1);
        const auto& p_tag = patches_tagged[0];
        std::cout << "    * minFold = 0.1 -> Edge tags size: " << p_tag.edge_tags.size() << std::endl;
        assert(p_tag.edge_tags.size() == 1 && "Hinge should be tagged as fold with minFold=0.1");

        // Run with default threshold = 1.0
        std::vector<unfold::UnfoldPatch> patches_default = unfold::Clusterer::unfold(mesh);
        assert(patches_default[0].edge_tags.empty() && "Default minFold=1.0 should suppress the fold tag");

    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] minFold suppression test failed: " << e.what() << std::endl;
        return 1;
    }

    // 6. Test grow and pair rules on Cube mesh
    std::cout << "  - Testing 'grow' and 'pair' rules on Cube mesh..." << std::endl;
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
        // Run with "grow" rule
        std::cout << "    * Running 'grow' rule..." << std::endl;
        std::vector<unfold::UnfoldPatch> patches_grow = unfold::Clusterer::unfold(mesh, 1.0, "grow");
        assert(patches_grow.size() == 1 && "Cube should unfold to 1 island under 'grow'");
        
        // Run with "pair" rule
        std::cout << "    * Running 'pair' rule..." << std::endl;
        std::vector<unfold::UnfoldPatch> patches_pair = unfold::Clusterer::unfold(mesh, 1.0, "pair");
        assert(patches_pair.size() == 1 && "Cube should unfold to 1 island under 'pair'");
        
    } catch (const std::exception& e) {
        std::cerr << "  - [CRASH] grow/pair rule test failed: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "✅ Unit Test Passed" << std::endl;
    return 0;
}
