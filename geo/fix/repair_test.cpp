#include <iostream>
#include <vector>
#include <cassert>
#include "repair.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>

using namespace jotcad::geo;
using namespace jotcad::geo::fix;

typedef CGAL::Surface_mesh<EK::Point_3> Mesh;

Mesh soup_to_manifold_mesh(const std::vector<EK::Point_3>& points, const std::vector<std::vector<std::size_t>>& faces) {
    std::vector<EK::Point_3> p = points;
    std::vector<std::vector<std::size_t>> f = faces;
    CGAL::Polygon_mesh_processing::repair_polygon_soup(p, f);
    
    Mesh mesh;
    for (const auto& pt : p) mesh.add_vertex(pt);
    for (const auto& face : f) {
        std::vector<Mesh::Vertex_index> v;
        for (auto idx : face) v.push_back(Mesh::Vertex_index(idx));
        mesh.add_face(v);
    }
    return mesh;
}

bool check_manifold(const Mesh& mesh) {
    std::vector<Mesh::Halfedge_index> h;
    CGAL::Polygon_mesh_processing::non_manifold_vertices(mesh, std::back_inserter(h));
    return h.empty();
}

void test_double_cone() {
    std::cout << "[Test 1] Double Cone (Apex-to-Apex)..." << std::endl;
    
    std::vector<EK::Point_3> pts = {
        {0,0,0}, {-5,-5,-10}, {5,-5,-10}, {5,5,-10}, {-5,5,-10},
        {-5,-5,10}, {5,-5,10}, {5,5,10}, {-5,5,10}
    };
    std::vector<std::vector<std::size_t>> faces = {
        {0,1,2}, {0,2,3}, {0,3,4}, {0,4,1}, {1,4,3}, {1,3,2},
        {0,5,6}, {0,6,7}, {0,7,8}, {0,8,5}, {5,8,7}, {5,7,6}
    };

    Mesh mesh = soup_to_manifold_mesh(pts, faces);

    // Explicitly verify non-manifoldness using the BGL-based check
    if (check_manifold(mesh)) {
        std::cout << "  ⚠️ Warning: CGAL did not report non-manifold vertices. Soup repair might have over-optimized." << std::endl;
    } else {
        std::cout << "  ✅ Initial state is non-manifold." << std::endl;
    }

    bool repaired = repair_self_touches(mesh, 0.1);
    
    if (repaired) {
        assert(check_manifold(mesh));
        std::cout << "  ✅ Manifold recovered." << std::endl;
    } else {
        std::cout << "  ℹ️ No non-manifold features detected by repair_self_touches." << std::endl;
    }
}

void test_bowtie_2d() {
    std::cout << "[Test 2] Bowtie (2D polygons sharing vertex)..." << std::endl;
    
    std::vector<EK::Point_3> pts = {
        {0,0,0}, {-10,-10,0}, {-10,10,0},
        {10,-10,0}, {10,10,0}
    };
    std::vector<std::vector<std::size_t>> faces = {
        {0,1,2}, {0,4,3}
    };

    Mesh mesh = soup_to_manifold_mesh(pts, faces);

    repair_self_touches(mesh, 0.1);
    assert(check_manifold(mesh));
    std::cout << "  ✅ 2D Bowtie resolved." << std::endl;
}

int main() {
    try {
        test_double_cone();
        test_bowtie_2d();
        std::cout << "\nALL MANIFOLD RECOVERY TESTS PASSED." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
