#include "matrix.h"
#include "decal_pipeline.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <cassert>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::decal;

// Helper to create a triangulated cube (12 faces)
ExactMesh create_test_cube() {
    ExactMesh m;
    auto v0 = m.add_vertex(EK::Point_3(0,0,0));
    auto v1 = m.add_vertex(EK::Point_3(1,0,0));
    auto v2 = m.add_vertex(EK::Point_3(1,1,0));
    auto v3 = m.add_vertex(EK::Point_3(0,1,0));
    auto v4 = m.add_vertex(EK::Point_3(0,0,1));
    auto v5 = m.add_vertex(EK::Point_3(1,0,1));
    auto v6 = m.add_vertex(EK::Point_3(1,1,1));
    auto v7 = m.add_vertex(EK::Point_3(0,1,1));
    
    // Bottom (Z=0)
    m.add_face(v0, v3, v1); m.add_face(v1, v3, v2);
    // Top (Z=1)
    m.add_face(v4, v5, v7); m.add_face(v5, v6, v7);
    // Front (Y=0)
    m.add_face(v0, v1, v4); m.add_face(v1, v5, v4);
    // Back (Y=1)
    m.add_face(v2, v3, v6); m.add_face(v3, v7, v6);
    // Left (X=0)
    m.add_face(v0, v4, v3); m.add_face(v3, v4, v7);
    // Right (X=1)
    m.add_face(v1, v2, v5); m.add_face(v2, v6, v5);
    return m;
}

void test_segmentation() {
    std::cout << "Testing Segmentation..." << std::endl;
    ExactMesh cube = create_test_cube();
    auto patches = segment_into_patches(cube);
    assert(patches.size() == 6);
    for (const auto& p : patches) {
        assert(p.faces.size() == 2);
    }
    std::cout << "  OK" << std::endl;
}

void test_unfolding() {
    std::cout << "Testing BFS Unfolding..." << std::endl;
    ExactMesh cube = create_test_cube();
    auto patches = segment_into_patches(cube);
    
    Matrix tf_relief_inv_in = Matrix::identity();
    unfold_patches(cube, patches, tf_relief_inv_in);
    
    // Verify each patch has an unfold matrix
    for (const auto& p : patches) {
        assert(p.unfold_tf != Matrix::identity() || p.id == 0); // Root might be identity
        
        // Verify mapping puts the patch on Z=0
        ExactMesh mapped = map_patch_to_uv_space(cube, p);
        for (auto v : mapped.vertices()) {
            double z = CGAL::to_double(mapped.point(v).z());
            assert(std::abs(z) < 1e-6);
        }
    }
    std::cout << "  OK" << std::endl;
}

void test_clipping() {
    std::cout << "Testing Physical Clipping..." << std::endl;
    
    // 1. Create a Relief (Open Mesh): A large flat sheet at Z=0
    ExactMesh relief;
    auto r0 = relief.add_vertex(EK::Point_3(-10,-10, 0));
    auto r1 = relief.add_vertex(EK::Point_3( 10,-10, 0));
    auto r2 = relief.add_vertex(EK::Point_3( 10, 10, 0));
    auto r3 = relief.add_vertex(EK::Point_3(-10, 10, 0));
    relief.add_face(r0, r1, r2);
    relief.add_face(r0, r2, r3);
    
    // 2. Create a Slab from a small flat patch
    ExactMesh patch;
    auto p0 = patch.add_vertex(EK::Point_3(0.2, 0.2, 0));
    auto p1 = patch.add_vertex(EK::Point_3(0.8, 0.2, 0));
    auto p2 = patch.add_vertex(EK::Point_3(0.8, 0.8, 0));
    auto p3 = patch.add_vertex(EK::Point_3(0.2, 0.8, 0));
    patch.add_face(p0, p1, p2);
    patch.add_face(p0, p2, p3);
    
    ExactMesh slab = build_extrusion_slab(patch, -5.0, 5.0);
    assert(CGAL::is_closed(slab));

    // 3. Clip the relief by the slab
    clip_relief_by_slab(relief, slab);

    // 4. Verify results
    assert(!relief.is_empty());
    for (auto v : relief.vertices()) {
        auto p = relief.point(v);
        double x = CGAL::to_double(p.x());
        double y = CGAL::to_double(p.y());
        assert(x >= 0.19 && x <= 0.81);
        assert(y >= 0.19 && y <= 0.81);
    }
    
    std::cout << "  OK" << std::endl;
}

void test_re_placement() {
    std::cout << "Testing Inverse Mapping (Re-placement)..." << std::endl;
    
    // Define an unfold matrix that translates by [10, 10, 10]
    Matrix unfold_tf = Matrix::translate(10, 10, 10);
    
    ExactMesh mesh;
    auto v0 = mesh.add_vertex(EK::Point_3(0, 0, 0));
    mesh.add_face(v0, v0, v0); // Degenerate but okay for vertex testing

    // Re-fold (inverse of unfold) should move origin to [-10, -10, -10]
    map_mesh_back_to_subject_space(mesh, unfold_tf);

    auto p = mesh.point(v0);
    assert(std::abs(CGAL::to_double(p.x()) - (-10.0)) < 1e-6);
    assert(std::abs(CGAL::to_double(p.y()) - (-10.0)) < 1e-6);
    assert(std::abs(CGAL::to_double(p.z()) - (-10.0)) < 1e-6);

    std::cout << "  OK" << std::endl;
}

int main() {
    test_segmentation();
    test_unfolding();
    test_clipping();
    test_re_placement();
    std::cout << "All decal pipeline tests passed." << std::endl;
    return 0;
}
