#include "test_base.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/orientation.h>

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

int main() {
    std::cout << "Starting Forensic Verification of Box Primitive..." << std::endl;

    Geometry box;
    double x_min = -5, x_max = 5;
    double y_min = -5, y_max = 5;
    double z_min = -5, z_max = 5;

    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)}); // 0
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)}); // 1
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_min)}); // 2
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)}); // 3
    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)}); // 4
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_max)}); // 5
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_max)}); // 6
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_max)}); // 7

    box.faces.push_back({{{3, 2, 1, 0}}}); // Bottom
    box.faces.push_back({{{4, 5, 6, 7}}}); // Top
    box.faces.push_back({{{0, 1, 5, 4}}}); // Front
    box.faces.push_back({{{1, 2, 6, 5}}}); // Right
    box.faces.push_back({{{2, 3, 7, 6}}}); // Back
    box.faces.push_back({{{3, 0, 4, 7}}}); // Left

    std::vector<EK::Point_3> pts;
    std::vector<std::vector<std::size_t>> faces;
    for (const auto& v : box.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
    for (const auto& f : box.faces) {
        std::vector<std::size_t> face;
        for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
        faces.push_back(face);
    }

    std::cout << "Initial: " << pts.size() << " vertices, " << faces.size() << " faces." << std::endl;

    CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
    std::cout << "After repair_polygon_soup: " << pts.size() << " vertices, " << faces.size() << " faces." << std::endl;

    bool oriented = CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);
    std::cout << "After orient_polygon_soup: " << (oriented ? "Orientable" : "NOT Orientable") << std::endl;

    ExactMesh mesh;
    std::vector<ExactMesh::Vertex_index> sm_v_indices;
    for (const auto& p : pts) sm_v_indices.push_back(mesh.add_vertex(p));
    for (const auto& f : faces) {
        std::vector<ExactMesh::Vertex_index> face_vs;
        for (auto idx : f) face_vs.push_back(sm_v_indices[idx]);
        if (mesh.add_face(face_vs) == ExactMesh::null_face()) {
            std::cout << "  - FAILED to add face to Surface_mesh!" << std::endl;
        }
    }

    std::cout << "Surface_mesh stats: " << mesh.number_of_vertices() << " vertices, " << mesh.number_of_faces() << " faces." << std::endl;
    std::cout << "  - Closed: " << (CGAL::is_closed(mesh) ? "YES" : "NO") << std::endl;
    
    std::vector<ExactMesh::Halfedge_index> h_nm;
    CGAL::Polygon_mesh_processing::non_manifold_vertices(mesh, std::back_inserter(h_nm));
    std::cout << "  - Non-manifold vertices: " << h_nm.size() << std::endl;

    return 0;
}
