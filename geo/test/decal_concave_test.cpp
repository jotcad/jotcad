#include "test_base.h"
#include "boolean/engine.h"
#include <CGAL/Polygon_mesh_processing/manifoldness.h>
#include <cassert>

using namespace jotcad::geo;

// Helper to create an L-bracket (concave corner)
Geometry create_l_bracket() {
    Geometry geo;
    // Bottom plate: [0, 20] x [0, 20] x [0, 2]
    // Side plate: [0, 20] x [0, 2] x [2, 20]
    // Concave corner at y=2, z=2 line

    // Bottom
    geo.vertices.push_back({FT(0), FT(0), FT(0)}); // 0
    geo.vertices.push_back({FT(20), FT(0), FT(0)}); // 1
    geo.vertices.push_back({FT(20), FT(20), FT(0)}); // 2
    geo.vertices.push_back({FT(0), FT(20), FT(0)}); // 3
    geo.vertices.push_back({FT(0), FT(0), FT(2)}); // 4
    geo.vertices.push_back({FT(20), FT(0), FT(2)}); // 5
    geo.vertices.push_back({FT(20), FT(20), FT(2)}); // 6
    geo.vertices.push_back({FT(0), FT(20), FT(2)}); // 7

    // Side
    geo.vertices.push_back({FT(0), FT(0), FT(20)}); // 8
    geo.vertices.push_back({FT(20), FT(0), FT(20)}); // 9
    geo.vertices.push_back({FT(20), FT(2), FT(20)}); // 10
    geo.vertices.push_back({FT(0), FT(2), FT(20)}); // 11
    
    // Faces for bottom plate
    geo.triangles.push_back({0, 1, 5}); geo.triangles.push_back({0, 5, 4});
    geo.triangles.push_back({1, 2, 6}); geo.triangles.push_back({1, 6, 5});
    geo.triangles.push_back({2, 3, 7}); geo.triangles.push_back({2, 7, 6});
    geo.triangles.push_back({3, 0, 4}); geo.triangles.push_back({3, 4, 7});
    geo.triangles.push_back({0, 3, 2}); geo.triangles.push_back({0, 2, 1}); // Bottom face
    
    // Faces for side plate (merged with bottom)
    // The concave corner is between the top of the bottom plate (v4-v5-v6-v7) 
    // and the "inside" face of the side plate.
    // Simplification: Let's just use two boxes and union them for a proper concave subject.
    return geo; 
}

void test_decal_concave_join() {
    MockVFS vfs("decal_concave");
    register_all_ops(&vfs);

    std::cout << "  - Creating Concave L-Bracket Subject..." << std::endl;
    // Box 1: [0, 20] x [0, 20] x [0, 2]
    fs::Selector b1_sel("jot/Box");
    b1_sel.parameters = {{"width", 20.0}, {"height", 20.0}, {"depth", 2.0}};
    b1_sel.output = "$out";
    Processor::execute(&vfs, b1_sel);
    Shape b1 = vfs.read<Shape>(b1_sel);

    // Box 2: [0, 20] x [0, 2] x [0, 20]
    fs::Selector b2_sel("jot/Box");
    b2_sel.parameters = {{"width", 20.0}, {"height", 2.0}, {"depth", 20.0}};
    b2_sel.output = "$out";
    Processor::execute(&vfs, b2_sel);
    Shape b2 = vfs.read<Shape>(b2_sel);
    b2.tf = Matrix::translate(0, 0, 2.0); // Move B2 to sit on top of B1, forming a concave corner at Y=2, Z=2

    fs::Selector union_sel("jot/join");
    union_sel.parameters = {{"$in", b1.to_json()}, {"tools", std::vector<nlohmann::json>{b2.to_json()}}};
    union_sel.output = "$out";
    Processor::execute(&vfs, union_sel);
    Shape subject = vfs.read<Shape>(union_sel);

    std::cout << "  - Creating Open Relief (Single Face)..." << std::endl;
    Geometry relief_geo;
    // Flat relief at Z=2.0
    relief_geo.vertices.push_back({FT(-100), FT(-100), FT(2.0)});
    relief_geo.vertices.push_back({FT( 100), FT(-100), FT(2.0)});
    relief_geo.vertices.push_back({FT( 100), FT( 100), FT(2.0)});
    relief_geo.vertices.push_back({FT(-100), FT( 100), FT(2.0)});
    relief_geo.triangles.push_back({0, 1, 2});
    relief_geo.triangles.push_back({0, 2, 3});

    Shape relief;
    relief.geometry = vfs.materialize<Geometry>(relief_geo);
    // Position relief so it intersects the subject corner
    relief.tf = Matrix::identity();

    std::cout << "  - Executing jot/decal with Attenuation..." << std::endl;
    fs::Selector decal_sel("jot/decal");
    decal_sel.parameters = {
        {"$in", subject.to_json()},
        {"relief", relief.to_json()},
        {"fade_radius", 5.0}
    };
    decal_sel.output = "$out";

    Processor::execute(&vfs, decal_sel);
    Shape result = vfs.read<Shape>(decal_sel);

    std::cout << "  - Rendering Result..." << std::endl;
    result.tf = Matrix::rotationX(-0.1) * Matrix::rotationY(0.125);
    auto png_data = Rasterizer::render_png(&vfs, result, 512, 512, 0.0, 0.0);
    if (!png_data.empty()) {
        std::filesystem::create_directories("actual");
        std::ofstream out("actual/decal_concave_wrap.png", std::ios::binary);
        out.write((const char*)png_data.data(), png_data.size());
        std::cout << "  📸 Saved actual/decal_concave_wrap.png" << std::endl;
    }
    
    Geometry res_geo = vfs.read<Geometry>(result.geometry.value());
    boolean::ExactMesh res_mesh = boolean::Engine::geometry_to_mesh(res_geo);

    std::cout << "  - Verifying Result Intersections..." << std::endl;
    std::vector<std::pair<boolean::ExactMesh::Face_index, boolean::ExactMesh::Face_index>> intersect_pairs;
    CGAL::Polygon_mesh_processing::self_intersections(res_mesh, std::back_inserter(intersect_pairs));
    
    if (!intersect_pairs.empty()) {
        std::cout << "    ❌ Found " << intersect_pairs.size() << " self-intersecting face pairs:" << std::endl;
        // Limit to first 5 pairs to avoid flooding
        int count = 0;
        for (const auto& p : intersect_pairs) {
            if (count++ >= 5) break;
            std::cout << "      Pair " << count << ": Face " << p.first << " and " << p.second << std::endl;
            
            auto print_face = [&](auto f) {
                std::cout << "        Face " << f << ": ";
                auto h = res_mesh.halfedge(f);
                for (auto v : res_mesh.vertices_around_face(h)) {
                    auto pt = res_mesh.point(v);
                    std::cout << "[" << CGAL::to_double(pt.x()) << "," << CGAL::to_double(pt.y()) << "," << CGAL::to_double(pt.z()) << "] ";
                }
                std::cout << std::endl;
            };
            print_face(p.first);
            print_face(p.second);
        }
    } else {
        std::cout << "    ✅ No self-intersections found." << std::endl;
    }

    std::cout << "  - Verifying Result Manifoldness (Unambiguous)..." << std::endl;
    // fix::is_geometry_unambiguous checks if the mesh is manifold and stable
    bool is_valid = fix::is_geometry_unambiguous(res_mesh);
    std::cout << "    - Is Manifold (Unambiguous): " << (is_valid ? "YES" : "NO") << std::endl;
    assert(is_valid);

    std::cout << "  ✅ Concave Join Test Passed (Manifold)" << std::endl;
}

int main() {
    test_decal_concave_join();
    return 0;
}
