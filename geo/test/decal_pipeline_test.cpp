#include "matrix.h"
#include "decal_pipeline.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>
#include <CGAL/Surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/measure.h>
#include <cassert>
#include <iostream>

using namespace jotcad;
using namespace jotcad::geo;
using namespace jotcad::geo::decal;

// Helper to create a triangulated cube (12 faces)
ExactMesh create_test_cube() {
    ExactMesh m;
    auto v0 = m.add_vertex(EK::Point_3(0,0,0));
    auto v1 = m.add_vertex(EK::Point_3(10,0,0));
    auto v2 = m.add_vertex(EK::Point_3(10,10,0));
    auto v3 = m.add_vertex(EK::Point_3(0,10,0));
    auto v4 = m.add_vertex(EK::Point_3(0,0,10));
    auto v5 = m.add_vertex(EK::Point_3(10,0,10));
    auto v6 = m.add_vertex(EK::Point_3(10,10,10));
    auto v7 = m.add_vertex(EK::Point_3(0,10,10));
    
    // Bottom (Z=0)
    m.add_face(v0, v3, v1); m.add_face(v1, v3, v2);
    // Top (Z=10)
    m.add_face(v4, v5, v7); m.add_face(v5, v6, v7);
    // Front (Y=0)
    m.add_face(v0, v1, v4); m.add_face(v1, v5, v4);
    // Back (Y=10)
    m.add_face(v2, v3, v6); m.add_face(v3, v7, v6);
    // Left (X=0)
    m.add_face(v0, v4, v3); m.add_face(v3, v4, v7);
    // Right (X=10)
    m.add_face(v1, v2, v5); m.add_face(v2, v6, v5);
    return m;
}

namespace jotcad {
namespace geo {
namespace decal {
ExactMesh create_cookie_cutter(const ExactMesh& relief, double z_min, double z_max) {
    ExactMesh footprint;
    std::map<ExactMesh::Vertex_index, ExactMesh::Vertex_index> v_map;
    std::vector<ExactMesh::Halfedge_index> border;
    CGAL::Polygon_mesh_processing::border_halfedges(relief, std::back_inserter(border));

    if (border.empty()) {
        CGAL::Bbox_3 bbox = CGAL::Polygon_mesh_processing::bbox(relief);
        auto v0 = footprint.add_vertex(EK::Point_3(bbox.xmin(), bbox.ymin(), 0));
        auto v1 = footprint.add_vertex(EK::Point_3(bbox.xmax(), bbox.ymin(), 0));
        auto v2 = footprint.add_vertex(EK::Point_3(bbox.xmax(), bbox.ymax(), 0));
        auto v3 = footprint.add_vertex(EK::Point_3(bbox.xmin(), bbox.ymax(), 0));
        footprint.add_face(v0, v1, v2, v3);
    } else {
        std::set<ExactMesh::Vertex_index> visited;
        for (auto start_he : border) {
            auto v_start = relief.source(start_he);
            if (visited.count(v_start)) continue;
            
            std::vector<ExactMesh::Vertex_index> loop;
            auto cur_he = start_he;
            do {
                auto v = relief.source(cur_he);
                visited.insert(v);
                if (!v_map.count(v)) {
                    auto p = relief.point(v);
                    v_map[v] = footprint.add_vertex(EK::Point_3(p.x(), p.y(), 0));
                }
                loop.push_back(v_map[v]);
                
                auto v_next = relief.target(cur_he);
                bool found = false;
                for (auto next_he : CGAL::halfedges_around_source(v_next, relief)) {
                    if (relief.is_border(next_he)) {
                        cur_he = next_he;
                        found = true;
                        break;
                    }
                }
                if (!found) break;
            } while (relief.source(cur_he) != v_start);
            
            if (loop.size() >= 3) {
                footprint.add_face(loop);
            }
        }
    }
    
    CGAL::Polygon_mesh_processing::triangulate_faces(footprint);
    ExactMesh precise_cutter;
    CGAL::Polygon_mesh_processing::extrude_mesh(footprint, precise_cutter, 
        Projector(footprint, precise_cutter, z_min), 
        Projector(footprint, precise_cutter, z_max));
    CGAL::Polygon_mesh_processing::triangulate_faces(precise_cutter);
    return precise_cutter;
}
} // namespace decal
} // namespace geo
} // namespace jotcad

void test_cookie_cutter() {
    std::cout << "Testing Cookie Cutter..." << std::endl;
    ExactMesh relief;
    auto v0 = relief.add_vertex(EK::Point_3(2, 2, 0));
    auto v1 = relief.add_vertex(EK::Point_3(8, 2, 0));
    auto v2 = relief.add_vertex(EK::Point_3(8, 8, 0));
    auto v3 = relief.add_vertex(EK::Point_3(2, 8, 0));
    relief.add_face(v0, v1, v2, v3);
    
    ExactMesh cutter = create_cookie_cutter(relief, -5, 5);
    assert(CGAL::is_closed(cutter));
    auto vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(cutter));
    std::cout << "  Cutter Volume: " << vol << std::endl;
    assert(std::abs(vol) > 1e-6);
    std::cout << "  OK" << std::endl;
}

void test_full_pipeline() {
    std::cout << "Testing Full Pipeline..." << std::endl;
    ExactMesh subject = create_test_cube();
    
    // Relief is a 5x5 square centered on the top face of the 10x10 cube
    ExactMesh relief;
    auto v0 = relief.add_vertex(EK::Point_3(2.5, 2.5, 11));
    auto v1 = relief.add_vertex(EK::Point_3(7.5, 2.5, 11));
    auto v2 = relief.add_vertex(EK::Point_3(7.5, 7.5, 11));
    auto v3 = relief.add_vertex(EK::Point_3(2.5, 7.5, 11));
    relief.add_face(v0, v1, v2, v3);
    
    Matrix tf_subject = Matrix::identity();
    Matrix tf_relief = Matrix::identity();
    
    ExactMesh result = apply_decal(subject, relief, tf_subject, tf_relief);
    
    assert(!result.is_empty());
    assert(CGAL::is_closed(result));
    
    // Expected volume: 1000 (cube) + 25 (skirting volume) = 1025
    auto vol = CGAL::to_double(CGAL::Polygon_mesh_processing::volume(result));
    std::cout << "  Result Volume: " << vol << std::endl;
    assert(std::abs(vol - 1025.0) < 1e-4);

    std::cout << "  OK" << std::endl;
}

int main() {
    test_cookie_cutter();
    test_full_pipeline();
    std::cout << "All decal pipeline tests passed." << std::endl;
    return 0;
}
