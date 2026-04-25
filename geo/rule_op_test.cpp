#include "test/test_base.h"
#include "impl/rasterizer.h"
#include "impl/matrix.h"
#include "rs/ruled_surfaces_base.h"
#include "../../fs/cpp/cid.h"
#include <chrono>

using namespace jotcad;
using namespace jotcad::geo;
using namespace fs;

// Helper to convert Geometry back to Mesh for validation
ruled_surfaces::Mesh geometry_to_mesh(const Geometry& geo) {
    ruled_surfaces::PolygonSoup soup;
    for (const auto& face : geo.faces) {
        for (const auto& loop : face.loops) {
            if (loop.size() == 3) {
                std::array<ruled_surfaces::PointCgal, 3> tri;
                for (int i = 0; i < 3; ++i) {
                    const auto& v = geo.vertices[loop[i]];
                    tri[i] = ruled_surfaces::PointCgal(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
                }
                soup.push_back(tri);
            }
        }
    }
    return ruled_surfaces::internal::soup_to_mesh(soup);
}

bool verify_and_save(const Geometry& geo, const std::string& label, const std::string& expected_hash = "", double ax = 0.0, double ay = 0.0) {
    ruled_surfaces::Mesh mesh = geometry_to_mesh(geo);
    bool manifold = ruled_surfaces::internal::is_manifold(mesh);
    bool self_intersecting = ruled_surfaces::internal::has_self_intersections(mesh);
    
    std::vector<uint8_t> png_bytes = Rasterizer::render_png(geo, 256, 256, ax, ay);
    std::string actual_hash = vfs_hash256(png_bytes);
    
    std::cout << "  🔍 " << label << " Results:" << std::endl;
    std::cout << "     Manifold: " << (manifold ? "✅ YES" : "❌ NO") << std::endl;
    std::cout << "     Self-Intersecting: " << (self_intersecting ? "❌ YES" : "✅ NO") << std::endl;
    std::cout << "     Visual Hash: " << actual_hash << std::endl;

    bool hash_match = true;
    if (!expected_hash.empty()) {
        if (actual_hash == expected_hash) {
            std::cout << "     Visual Match: ✅ PASS" << std::endl;
        } else {
            std::cout << "     Visual Match: ❌ FAIL (Expected: " << expected_hash << ")" << std::endl;
            hash_match = false;
        }
    }

    std::string filename = "rule_test_" + label + ".actual.png";
    std::ofstream out(filename, std::ios::binary);
    out.write((char*)png_bytes.data(), png_bytes.size());
    out.close();

    return manifold && hash_match;
}

int main() {
    MockVFS vfs("rule");
    register_all_ops(&vfs);
    
    auto make_polyline_3d = [&](double y_offset, double z_offset) {
        Geometry geo;
        geo.vertices.push_back({FT(0.0), FT(y_offset), FT(z_offset)});
        geo.vertices.push_back({FT(10.0), FT(y_offset + 5.0), FT(z_offset)});
        geo.vertices.push_back({FT(20.0), FT(y_offset), FT(z_offset)});
        geo.segments.push_back({0, 1});
        geo.segments.push_back({1, 2});
        Shape s;
        s.geometry = vfs.write_anonymous<Geometry>(geo);
        return s;
    };

    auto make_loop = [&](double z_offset, double size) {
        Geometry geo;
        geo.vertices.push_back({FT(0.0), FT(0.0), FT(z_offset)});
        geo.vertices.push_back({FT(size), FT(0.0), FT(z_offset)});
        geo.vertices.push_back({FT(size), FT(size), FT(z_offset)});
        geo.vertices.push_back({FT(0.0), FT(size), FT(z_offset)});
        Geometry::Face f;
        f.loops.push_back({0, 1, 2, 3});
        geo.faces.push_back(f);
        Shape s;
        s.geometry = vfs.write_anonymous<Geometry>(geo);
        return s;
    };

    bool all_passed = true;
    auto start = std::chrono::high_resolution_clock::now();

    // Test 1: Polyline
    std::cout << "[Test 1] Polyline to Polyline..." << std::endl;
    Shape p1 = make_polyline_3d(0.0, 0.0);
    Shape p2 = make_polyline_3d(10.0, 10.0);
    Selector sel1{"jot/Rule", {{"$a", p1}, {"$b", p2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel1); // Trigger RuleOp
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel1).geometry.value());
        if (!verify_and_save(geo, "polyline", "f80b2ae0a2895783b9678e72d07aa6a8a32895b5033daad335860ca278301a35")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 1 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 2: Loop
    std::cout << "[Test 2] Loop to Loop..." << std::endl;
    Shape l1 = make_loop(0.0, 10.0);
    Shape l2 = make_loop(10.0, 15.0);
    Selector sel2{"jot/Rule", {{"$a", l1}, {"$b", l2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel2);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel2).geometry.value());
        if (!verify_and_save(geo, "loop", "f68b7d05a36bf5f25ad095011ca25634f5c91d0acb66f9e090227e75c34a298f")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 2 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 3: Holes
    std::cout << "[Test 3] Holes..." << std::endl;
    Geometry frame_geo;
    frame_geo.vertices.push_back({FT(0), FT(0), FT(0)});
    frame_geo.vertices.push_back({FT(100), FT(0), FT(0)});
    frame_geo.vertices.push_back({FT(100), FT(100), FT(0)});
    frame_geo.vertices.push_back({FT(0), FT(100), FT(0)});
    frame_geo.vertices.push_back({FT(40), FT(40), FT(0)});
    frame_geo.vertices.push_back({FT(60), FT(40), FT(0)});
    frame_geo.vertices.push_back({FT(60), FT(60), FT(0)});
    frame_geo.vertices.push_back({FT(40), FT(60), FT(0)});
    Geometry::Face frame_face;
    frame_face.loops.push_back({0, 1, 2, 3});
    frame_face.loops.push_back({4, 5, 6, 7});
    frame_geo.faces.push_back(frame_face);
    Shape s_h1; s_h1.geometry = vfs.write_anonymous<Geometry>(frame_geo);
    Shape s_h2 = s_h1; s_h2.tf = Matrix::translate(0, 0, 50).to_vec();
    Selector sel3{"jot/Rule", {{"$a", s_h1}, {"$b", s_h2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel3);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel3).geometry.value());
        if (!verify_and_save(geo, "holes", "bf2c6ddb6336f00c5409df89c95c97b9e21f9801dc37cb5a806ac579554f425a")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 3 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 4: Stress
    std::cout << "[Test 4] Stress..." << std::endl;
    Geometry g_st1, g_st2;
    for (int i = 0; i < 100; ++i) {
        double a = (i / 100.0) * 2.0 * M_PI;
        g_st1.vertices.push_back({FT(50 * std::cos(a)), FT(50 * std::sin(a)), FT(0)});
        g_st2.vertices.push_back({FT(50 * std::cos(a)), FT(50 * std::sin(a)), FT(100)});
    }
    std::vector<int> l_st; for (int i = 0; i < 100; ++i) l_st.push_back(i);
    Geometry::Face f_st; f_st.loops.push_back(l_st);
    g_st1.faces.push_back(f_st); g_st2.faces.push_back(f_st);
    Shape s_st1, s_st2;
    s_st1.geometry = vfs.write_anonymous<Geometry>(g_st1);
    s_st2.geometry = vfs.write_anonymous<Geometry>(g_st2);
    Selector sel4{"jot/Rule", {{"$a", s_st1}, {"$b", s_st2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel4);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel4).geometry.value());
        if (!verify_and_save(geo, "stress", "70227caef6349a395359ad63e93d55a7b8e1f05e4b64478a2b4d7f37d97721ec")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 4 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 5: Skewed
    std::cout << "[Test 5] Skewed..." << std::endl;
    Shape l_bottom = make_loop(0.0, 50.0);
    Shape l_top = make_loop(100.0, 50.0);
    l_top.tf = Matrix::translate(150.0, 50.0, 100.0).to_vec();
    Selector sel5{"jot/Rule", {{"$a", l_bottom}, {"$b", l_top}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel5);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel5).geometry.value());
        if (!verify_and_save(geo, "skewed", "aa9b18be2e4bad3417e71e97d53d7322e19b10120b9b9cf769a236b4d6b9ed0f")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 5 Failed: " << e.what() << std::endl; all_passed = false; }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Tests completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms." << std::endl;
    return all_passed ? 0 : 1;
}
