#include "test/test_base.h"
#include "rasterizer.h"
#include "matrix.h"
#include "rs/ruled_surfaces_base.h"
#include "../../fs/cpp/cid.h"
#include <chrono>

using namespace jotcad;
using namespace jotcad::geo;
using namespace fs;

// Helper to convert Geometry back to Mesh for validation
ruled_surfaces::Mesh geometry_to_mesh(const Geometry& geo) {
    ruled_surfaces::PolygonSoup soup;
    for (const auto& t : geo.triangles) {
        std::array<ruled_surfaces::PointCgal, 3> tri;
        for (int i = 0; i < 3; ++i) {
            const auto& v = geo.vertices[t[i]];
            tri[i] = ruled_surfaces::PointCgal(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
        }
        soup.push_back(tri);
    }
    for (const auto& face : geo.faces) {
        for (const auto& loop : face.loops) {
            if (loop.size() == 3) {
                bool represented = false;
                if (!geo.triangles.empty()) {
                    std::set<int> f_verts;
                    for (const auto& l : face.loops) {
                        f_verts.insert(l.begin(), l.end());
                    }
                    for (const auto& t : geo.triangles) {
                        if (f_verts.count(t[0]) && f_verts.count(t[1]) && f_verts.count(t[2])) {
                            represented = true;
                            break;
                        }
                    }
                }
                if (!represented) {
                    std::array<ruled_surfaces::PointCgal, 3> tri;
                    for (int i = 0; i < 3; ++i) {
                        const auto& v = geo.vertices[loop[i]];
                        tri[i] = ruled_surfaces::PointCgal(CGAL::to_double(v.x), CGAL::to_double(v.y), CGAL::to_double(v.z));
                    }
                    soup.push_back(tri);
                }
            }
        }
    }
    return ruled_surfaces::internal::soup_to_mesh(soup);
}

bool verify_and_save(VFSNode* vfs, const Geometry& geo, const std::string& label, const std::string& expected_hash = "", double ax = 0.0, double ay = 0.0) {
    ruled_surfaces::Mesh mesh = geometry_to_mesh(geo);
    bool manifold = ruled_surfaces::internal::is_manifold(mesh);
    bool self_intersecting = ruled_surfaces::internal::has_self_intersections(mesh);
    
    Shape s;
    s.geometry = vfs->materialize<Geometry>(geo);
    std::vector<uint8_t> png_bytes = Rasterizer::render_png(vfs, s, 256, 256, ax, ay);
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
        s.geometry = vfs.materialize<Geometry>(geo);
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
        s.geometry = vfs.materialize<Geometry>(geo);
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
        if (!verify_and_save(&vfs, geo, "polyline", "01b5ff1dbacbcf14d27b028aacf09cc2827e410ea1ac24626359ca61bd3c8b76")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 1 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 2: Loop
    std::cout << "[Test 2] Loop to Loop..." << std::endl;
    Shape l1 = make_loop(0.0, 10.0);
    Shape l2 = make_loop(10.0, 15.0);
    Selector sel2{"jot/Rule", {{"$a", l1}, {"$b", l2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel2);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel2).geometry.value());
        if (!verify_and_save(&vfs, geo, "loop", "2570a43dd45ee63305c23e9cebab4544fe90682902b99275ae8d046753ac9e62")) all_passed = false;
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
    Shape s_h1; s_h1.geometry = vfs.materialize<Geometry>(frame_geo);
    Shape s_h2 = s_h1; s_h2.tf = Matrix::translate(0, 0, 50);
    Selector sel3{"jot/Rule", {{"$a", s_h1}, {"$b", s_h2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel3);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel3).geometry.value());
        if (!verify_and_save(&vfs, geo, "holes", "4365364d6667c84a6e192223392f006bc9fe06654a02918cc00387bc5ddd472a")) all_passed = false;
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
    s_st1.geometry = vfs.materialize<Geometry>(g_st1);
    s_st2.geometry = vfs.materialize<Geometry>(g_st2);
    Selector sel4{"jot/Rule", {{"$a", s_st1}, {"$b", s_st2}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel4);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel4).geometry.value());
        if (!verify_and_save(&vfs, geo, "stress", "40a596a46e9d50b3aaa7ff1329e123a84d63237aefa467d5dcf746625b38a7b6")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 4 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 5: Skewed
    std::cout << "[Test 5] Skewed..." << std::endl;
    Shape l_bottom = make_loop(0.0, 50.0);
    Shape l_top = make_loop(100.0, 50.0);
    l_top.tf = Matrix::translate(150.0, 50.0, 100.0);
    Selector sel5{"jot/Rule", {{"$a", l_bottom}, {"$b", l_top}}, "$out"};
    try {
        vfs.read<std::vector<uint8_t>>(sel5);
        Geometry geo = vfs.read<Geometry>(vfs.read<Shape>(sel5).geometry.value());
        if (!verify_and_save(&vfs, geo, "skewed", "2a358a5737c53186e0552d6ef2b3ae63ce6d272167d8fff3a4237e29a28299db")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 5 Failed: " << e.what() << std::endl; all_passed = false; }

    // Test 6: Spun Surface
    std::cout << "[Test 6] Spun Surface..." << std::endl;
    try {
        Geometry p_line_geo;
        p_line_geo.vertices.push_back({FT(0.0), FT(0.0), FT(0.0)});
        p_line_geo.vertices.push_back({FT(1.0), FT(0.0), FT(1.0)});
        p_line_geo.vertices.push_back({FT(1.0), FT(0.0), FT(4.0)});
        p_line_geo.vertices.push_back({FT(2.0), FT(0.0), FT(5.0)});
        p_line_geo.segments.push_back({0, 1});
        p_line_geo.segments.push_back({1, 2});
        p_line_geo.segments.push_back({2, 3});
        Shape p_line;
        p_line.geometry = vfs.materialize<Geometry>(p_line_geo);

        Selector spin_sel{"jot/spin", {{"$in", p_line}, {"start", 0.0}, {"end", 1.0}, {"resolution", 32}, {"zag", 0.0}}, "$out"};
        Shape s1 = vfs.read<Shape>(spin_sel);
        
        Shape s2 = s1;
        s2.tf = Matrix::translate(0, 0, 15) * Matrix::scale(1, 1, -1);

        Selector rule_sel{"jot/Rule", {{"$a", s1}, {"$b", s2}}, "$out"};
        vfs.read<std::vector<uint8_t>>(rule_sel);
        Geometry res_geo = vfs.read<Geometry>(vfs.read<Shape>(rule_sel).geometry.value());
        if (!verify_and_save(&vfs, res_geo, "spun_surface", "04e8cbe17de2d28e47e82dfde2c3be84e39b675d645a11c94c40eca9aa9db1be")) all_passed = false;
    } catch (const std::exception& e) { std::cerr << "Test 6 Failed: " << e.what() << std::endl; all_passed = false; }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Tests completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms." << std::endl;
    return all_passed ? 0 : 1;
}
