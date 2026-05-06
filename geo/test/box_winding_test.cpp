#include "test_base.h"
#include "boolean/engine.h"

using namespace jotcad::geo;
using namespace jotcad::geo::boolean;

int main() {
    std::cout << "Testing if orient_polygon_soup flips valid Box winding..." << std::endl;

    Geometry box;
    double x_min = -5, x_max = 5;
    double y_min = -5, y_max = 5;
    double z_min = -5, z_max = 5;

    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_min)});
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_min)});
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_min)});
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_min)});
    box.vertices.push_back({FT(x_min), FT(y_min), FT(z_max)});
    box.vertices.push_back({FT(x_max), FT(y_min), FT(z_max)});
    box.vertices.push_back({FT(x_max), FT(y_max), FT(z_max)});
    box.vertices.push_back({FT(x_min), FT(y_max), FT(z_max)});

    box.faces.push_back({{{3, 2, 1, 0}}});
    box.faces.push_back({{{4, 5, 6, 7}}});
    box.faces.push_back({{{0, 1, 5, 4}}});
    box.faces.push_back({{{1, 2, 6, 5}}});
    box.faces.push_back({{{2, 3, 7, 6}}});
    box.faces.push_back({{{3, 0, 4, 7}}});

    std::vector<EK::Point_3> pts;
    std::vector<std::vector<std::size_t>> faces;
    for (const auto& v : box.vertices) pts.push_back(EK::Point_3(v.x, v.y, v.z));
    for (const auto& f : box.faces) {
        std::vector<std::size_t> face;
        for (int idx : f.loops[0]) face.push_back((std::size_t)idx);
        faces.push_back(face);
        // SIMULATE REDUNDANCY: Add triangles that overlap the quads (what materialized geo does)
        faces.push_back({(std::size_t)f.loops[0][0], (std::size_t)f.loops[0][1], (std::size_t)f.loops[0][2]});
        faces.push_back({(std::size_t)f.loops[0][0], (std::size_t)f.loops[0][2], (std::size_t)f.loops[0][3]});
    }

    std::cout << "Initial: " << pts.size() << " vertices, " << faces.size() << " faces (REDUNDANT)." << std::endl;

    auto initial_faces = faces;

    CGAL::Polygon_mesh_processing::repair_polygon_soup(pts, faces);
    CGAL::Polygon_mesh_processing::orient_polygon_soup(pts, faces);

    bool flipped = false;
    for (size_t i = 0; i < faces.size(); ++i) {
        if (faces[i] != initial_faces[i]) {
            std::cout << "  - Face " << i << " was flipped or modified!" << std::endl;
            flipped = true;
        }
    }

    if (!flipped) {
        std::cout << "  - No faces flipped. Winding preserved." << std::endl;
    }

    ExactMesh mesh;
    std::vector<ExactMesh::Vertex_index> sm_v_indices;
    for (const auto& p : pts) sm_v_indices.push_back(mesh.add_vertex(p));
    for (size_t i = 0; i < faces.size(); ++i) {
        std::vector<ExactMesh::Vertex_index> face_vs;
        for (auto idx : faces[i]) face_vs.push_back(sm_v_indices[idx]);
        if (mesh.add_face(face_vs) == ExactMesh::null_face()) {
            std::cerr << "FAIL: Could not add face " << i << " to Surface_mesh!" << std::endl;
            return 1;
        }
    }

    std::cout << "  - Closed: " << (CGAL::is_closed(mesh) ? "YES" : "NO") << std::endl;

    return 0;
}
